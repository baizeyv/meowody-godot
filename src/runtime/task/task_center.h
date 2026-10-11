#pragma once
#include "runtime/main_thread/main_thread_center.h"
#include <variant>
#include <cstdint>
#include <vector>

namespace meowody {
enum class TaskStatus { Queued, Running, AwaitingMainThread, Completed, Failed, Cancelled, Rejected };
enum class TaskError { None, NotReady, InvalidArgument, QueueFull, Closed, Failed, Cancelled };
using TaskValue = std::variant<std::monostate, bool, int64_t, double, std::string, std::vector<uint8_t>, std::shared_ptr<void>>;
struct TaskResult {
	TaskError error = TaskError::None;
	TaskValue value;
	std::string message;
	bool ok() const { return error == TaskError::None; }
};
namespace detail { struct TaskState; struct TaskPool; }
class TaskContext {
	std::shared_ptr<detail::TaskState> state;
	explicit TaskContext(std::shared_ptr<detail::TaskState> p) : state(std::move(p)) {}
	friend class TaskCenter;
public:
	bool is_cancelled() const;
};
class TaskHandle {
	std::shared_ptr<detail::TaskState> state;
	explicit TaskHandle(std::shared_ptr<detail::TaskState> p) : state(std::move(p)) {}
	friend class TaskCenter;
public:
	TaskHandle() = default;
	bool valid() const { return bool(state); }
	TaskStatus status() const;
	bool cancel() const;
	bool is_cancel_requested() const;
	TaskResult result() const;
};
class TaskCenter {
	MainThreadCenter &main_thread;
	const size_t worker_count;
	std::shared_ptr<detail::TaskPool> pool;
	std::vector<std::thread> workers;
	static void work(std::shared_ptr<detail::TaskPool> pool, MainThreadCenter &main_thread);
public:
	explicit TaskCenter(MainThreadCenter &main_thread, size_t worker_count = 0, size_t queue_capacity = 256);
	~TaskCenter();
	TaskCenter(const TaskCenter &) = delete;
	TaskCenter &operator=(const TaskCenter &) = delete;
	TaskHandle submit(Delegate<TaskResult(const TaskContext &)> work, Delegate<void(const TaskResult &)> completed = {});
	// Owning main thread only; cancels queued/running jobs and joins workers.
	bool shutdown();
};
} // namespace meowody
