#include "main_thread_center.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>

namespace meowody {
DispatchStatus DispatchHandle::status() const { return state ? state->status.load() : DispatchStatus::Rejected; }
bool DispatchHandle::cancel() const {
	if (!state) return false;
	auto expected = DispatchStatus::Pending;
	return state->status.compare_exchange_strong(expected, DispatchStatus::Cancelled);
}
std::string DispatchHandle::error() const {
	if (!state) return "Invalid dispatch handle";
	std::lock_guard lock(state->mutex);
	return state->error;
}
MainThreadCenter::~MainThreadCenter() { shutdown(); }
DispatchHandle MainThreadCenter::post(Delegate<void()> callback) {
	auto state = std::make_shared<detail::DispatchState>();
	{
		std::lock_guard lock(mutex);
		if (!callback || closed || queue.size() >= capacity) {
			state->error = !callback ? "Empty callback" : closed ? "MainThreadCenter closed" : "Main thread queue full";
			state->status = DispatchStatus::Rejected;
		} else {
			queue.push_back({state, std::move(callback)});
		}
	}
	return DispatchHandle(state);
}
size_t MainThreadCenter::update(size_t max_callbacks, double max_milliseconds) {
	if (!is_main_thread() || updating || !std::isfinite(max_milliseconds) || max_milliseconds < 0) return 0;
	updating = true;
	struct Guard { bool &value; ~Guard() { value = false; } } guard{updating};
	size_t count;
	{ std::lock_guard lock(mutex); count = closed ? 0 : std::min(max_callbacks, queue.size()); }
	if (count == 0) return 0;
	auto start = std::chrono::steady_clock::now();
	size_t executed = 0;
	for (size_t i = 0; i < count; ++i) {
		if (max_milliseconds > 0 && std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= max_milliseconds) break;
		Entry entry;
		{
			std::lock_guard lock(mutex);
			if (closed || queue.empty()) break;
			entry = std::move(queue.front()); queue.pop_front();
		}
		auto expected = DispatchStatus::Pending;
		if (!entry.state->status.compare_exchange_strong(expected, DispatchStatus::Running)) continue;
		++executed;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		try {
#endif
			entry.callback();
			entry.state->status = DispatchStatus::Completed;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		} catch (const std::exception &error) {
			std::lock_guard lock(entry.state->mutex);
			entry.state->error = error.what(); entry.state->status = DispatchStatus::Failed;
		} catch (...) {
			std::lock_guard lock(entry.state->mutex);
			entry.state->error = "Unknown callback exception"; entry.state->status = DispatchStatus::Failed;
		}
#endif
	}
	return executed;
}
void MainThreadCenter::shutdown() {
	std::deque<Entry> pending;
	{ std::lock_guard lock(mutex); closed = true; pending.swap(queue); }
	for (auto &entry : pending) {
		auto expected = DispatchStatus::Pending;
		entry.state->status.compare_exchange_strong(expected, DispatchStatus::Cancelled);
	}
}
bool MainThreadCenter::is_closed() const { std::lock_guard lock(mutex); return closed; }
size_t MainThreadCenter::pending_count() const { std::lock_guard lock(mutex); return queue.size(); }
} // namespace meowody
