#include "task_center.h"
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <unordered_set>

namespace meowody {
namespace detail {
struct TaskState {
	mutable std::mutex mutex;
	std::atomic<TaskStatus> status{TaskStatus::Queued};
	std::atomic<bool> cancel_requested{false};
	TaskResult result;
};
struct TaskJob {
	std::shared_ptr<TaskState> state;
	Delegate<TaskResult(const TaskContext &)> work;
	Delegate<void(const TaskResult &)> completed;
};
struct TaskPool {
	std::mutex mutex;
	std::condition_variable wake;
	std::deque<TaskJob> queue;
	std::unordered_set<std::shared_ptr<TaskState>> inflight;
	size_t capacity;
	bool closed = false;
};
}
namespace {
bool terminal(TaskStatus status) {
	return status == TaskStatus::Completed || status == TaskStatus::Failed || status == TaskStatus::Cancelled || status == TaskStatus::Rejected;
}
void cancel_state(detail::TaskState &state) {
	state.cancel_requested = true;
	if (state.status != TaskStatus::Running) {
		state.status = TaskStatus::Cancelled;
		state.result = {TaskError::Cancelled, {}, "Task cancelled"};
	}
}
void retire(const std::shared_ptr<detail::TaskPool> &pool, const std::shared_ptr<detail::TaskState> &state) {
	std::lock_guard lock(pool->mutex); pool->inflight.erase(state);
}
struct CompletionLifetime {
	std::shared_ptr<detail::TaskState> state;
	std::shared_ptr<detail::TaskPool> pool;
	CompletionLifetime(std::shared_ptr<detail::TaskState> s, std::shared_ptr<detail::TaskPool> p) : state(std::move(s)), pool(std::move(p)) {}
	~CompletionLifetime() {
		{
			std::lock_guard lock(state->mutex);
			if (state->status == TaskStatus::AwaitingMainThread) cancel_state(*state);
		}
		retire(pool, state);
	}
};
}
bool TaskContext::is_cancelled() const { return state->cancel_requested.load(); }
TaskStatus TaskHandle::status() const { return state ? state->status.load() : TaskStatus::Rejected; }
bool TaskHandle::is_cancel_requested() const { return state && state->cancel_requested.load(); }
bool TaskHandle::cancel() const {
	if (!state) return false;
	std::lock_guard lock(state->mutex);
	if (terminal(state->status) || state->cancel_requested) return false;
	cancel_state(*state); return true;
}
TaskResult TaskHandle::result() const {
	if (!state) return {TaskError::InvalidArgument, {}, "Invalid task handle"};
	std::lock_guard lock(state->mutex);
	if (state->status == TaskStatus::Queued || state->status == TaskStatus::Running) return {TaskError::NotReady, {}, "Task still running"};
	return state->result;
}
TaskCenter::TaskCenter(MainThreadCenter &main, size_t count, size_t capacity)
	: main_thread(main), worker_count(count ? count : std::min(4u, std::max(1u, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1))), pool(std::make_shared<detail::TaskPool>()) {
	pool->capacity = capacity;
}
TaskCenter::~TaskCenter() { shutdown(); }
TaskHandle TaskCenter::submit(Delegate<TaskResult(const TaskContext &)> work, Delegate<void(const TaskResult &)> completed) {
	auto state = std::make_shared<detail::TaskState>();
	{
		std::lock_guard lock(pool->mutex);
		if (!work || pool->closed || pool->queue.size() >= pool->capacity) {
			state->result = {!work ? TaskError::InvalidArgument : pool->closed ? TaskError::Closed : TaskError::QueueFull, {}, "Task submission rejected"};
			state->status = TaskStatus::Rejected;
		} else {
			pool->inflight.insert(state);
			pool->queue.push_back({state, std::move(work), std::move(completed)});
			if (workers.empty()) {
				for (size_t i = 0; i < worker_count; ++i)
					workers.emplace_back([p = pool, &main = main_thread] { TaskCenter::work(p, main); });
			}
		}
	}
	pool->wake.notify_one();
	return TaskHandle(state);
}
void TaskCenter::work(std::shared_ptr<detail::TaskPool> pool, MainThreadCenter &main) {
	for (;;) {
		detail::TaskJob job;
		{
			std::unique_lock lock(pool->mutex);
			pool->wake.wait(lock, [&] { return pool->closed || !pool->queue.empty(); });
			if (pool->closed) return;
			job = std::move(pool->queue.front()); pool->queue.pop_front();
		}
		bool cancelled;
		{
			std::lock_guard lock(job.state->mutex);
			cancelled = job.state->cancel_requested;
			if (!cancelled) job.state->status = TaskStatus::Running;
		}
		if (cancelled) { retire(pool, job.state); continue; }
		TaskResult result;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		try {
#endif
			result = job.work(TaskContext(job.state));
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		} catch (const std::exception &e) { result = {TaskError::Failed, {}, e.what()}; }
		catch (...) { result = {TaskError::Failed, {}, "Unknown task exception"}; }
#endif
		{
			std::lock_guard lock(job.state->mutex);
			if (job.state->cancel_requested) {
				job.state->result = {TaskError::Cancelled, {}, "Task cancelled"};
				job.state->status = TaskStatus::Cancelled;
				cancelled = true;
			} else {
				job.state->result = std::move(result);
				job.state->status = job.completed ? TaskStatus::AwaitingMainThread : job.state->result.ok() ? TaskStatus::Completed : TaskStatus::Failed;
			}
		}
		if (cancelled || !job.completed) { retire(pool, job.state); continue; }
		auto lifetime = std::make_shared<CompletionLifetime>(job.state, pool);
		auto dispatched = main.post([state = job.state, pool, lifetime, callback = std::move(job.completed)]() mutable {
			bool deliver;
			{
				std::lock_guard lock(state->mutex);
				deliver = !state->cancel_requested;
				if (deliver) {
					state->status = state->result.ok() ? TaskStatus::Completed : TaskStatus::Failed;
				}
			}
			retire(pool, state);
			if (!deliver) return;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			try {
#endif
				callback(state->result);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			} catch (const std::exception &e) {
				std::lock_guard lock(state->mutex);
				state->result = {TaskError::Failed, {}, e.what()}; state->status = TaskStatus::Failed;
			} catch (...) {
				std::lock_guard lock(state->mutex);
				state->result = {TaskError::Failed, {}, "Unknown completion exception"}; state->status = TaskStatus::Failed;
			}
#endif
		});
		if (dispatched.status() == DispatchStatus::Rejected) {
			{
				std::lock_guard lock(job.state->mutex);
				if (!job.state->cancel_requested) {
					job.state->result = {main.is_closed() ? TaskError::Closed : TaskError::QueueFull, {}, dispatched.error()};
					job.state->status = TaskStatus::Rejected;
				}
			}
			retire(pool, job.state);
		}
	}
}
bool TaskCenter::shutdown() {
	if (!main_thread.is_main_thread()) return false;
	std::deque<detail::TaskJob> pending;
	{
		std::lock_guard lock(pool->mutex);
		pool->closed = true;
		for (const auto &state : pool->inflight) {
			std::lock_guard state_lock(state->mutex);
			if (!terminal(state->status)) cancel_state(*state);
		}
		pending.swap(pool->queue);
	}
	pool->wake.notify_all();
	for (auto &worker : workers) if (worker.joinable()) worker.join();
	workers.clear();
	{ std::lock_guard lock(pool->mutex); pool->inflight.clear(); }
	return true;
}
} // namespace meowody
