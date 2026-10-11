#pragma once
#include "runtime/core/delegate.hpp"
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace meowody {
enum class DispatchStatus { Pending, Running, Completed, Cancelled, Failed, Rejected };
namespace detail {
struct DispatchState {
	std::atomic<DispatchStatus> status{DispatchStatus::Pending};
	std::mutex mutex;
	std::string error;
};
}
class DispatchHandle {
	std::shared_ptr<detail::DispatchState> state;
	explicit DispatchHandle(std::shared_ptr<detail::DispatchState> p) : state(std::move(p)) {}
	friend class MainThreadCenter;
public:
	DispatchHandle() = default;
	bool valid() const { return bool(state); }
	DispatchStatus status() const;
	bool cancel() const;
	std::string error() const;
};
class MainThreadCenter {
	struct Entry { std::shared_ptr<detail::DispatchState> state; Delegate<void()> callback; };
	const std::thread::id owner = std::this_thread::get_id();
	const size_t capacity;
	mutable std::mutex mutex;
	std::deque<Entry> queue;
	bool closed = false;
	bool updating = false;
public:
	explicit MainThreadCenter(size_t queue_capacity = 4096) : capacity(queue_capacity) {}
	~MainThreadCenter();
	MainThreadCenter(const MainThreadCenter &) = delete;
	MainThreadCenter &operator=(const MainThreadCenter &) = delete;
	DispatchHandle post(Delegate<void()> callback);
	size_t update(size_t max_callbacks = 128, double max_milliseconds = 2.0);
	void shutdown();
	bool is_main_thread() const { return owner == std::this_thread::get_id(); }
	size_t pending_count() const;
	bool is_closed() const;
};
} // namespace meowody
