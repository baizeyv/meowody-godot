#include "runtime/main_thread/main_thread_center.h"
#include "runtime/task/task_center.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace meowody;
namespace {
template <class Condition>
void until(Condition condition, MainThreadCenter *main = nullptr) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!condition()) {
        if (main) main->update(128, 0);
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}
void dispatch_tests() {
    MainThreadCenter main(2);
    std::vector<int> order;
    auto first = main.post([&] { order.push_back(1); main.post([&] { order.push_back(3); }); assert(main.update() == 0); });
    auto second = main.post([&] { order.push_back(2); });
    assert(main.post([] {}).status() == DispatchStatus::Rejected);
    assert(second.cancel()); assert(!second.cancel());
    std::thread other([&] { assert(main.update() == 0); }); other.join();
    assert(main.update(2, 0) == 1);
    assert(first.status() == DispatchStatus::Completed && (order == std::vector<int>{1}));
    assert(main.update(2, 0) == 1 && (order == std::vector<int>{1, 3}));
    auto pending = main.post([] {}); main.shutdown();
    assert(pending.status() == DispatchStatus::Cancelled && main.is_closed());
    assert(main.post([] {}).status() == DispatchStatus::Rejected);
    MainThreadCenter concurrent;
    std::atomic<int> posted{0};
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) producers.emplace_back([&] {
        for (int j = 0; j < 100; ++j) concurrent.post([&] { ++posted; });
    });
    for (auto &thread : producers) thread.join();
    while (concurrent.pending_count()) concurrent.update(17, 0);
    assert(posted == 400);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    auto failed = concurrent.post([] { throw std::runtime_error("dispatch failure"); });
    concurrent.update();
    assert(failed.status() == DispatchStatus::Failed && failed.error() == "dispatch failure");
#endif
}
void task_tests() {
    MainThreadCenter main;
    TaskCenter tasks(main, 1, 1);
    bool delivered = false;
    auto owner = std::this_thread::get_id();
    auto number = tasks.submit([&](const TaskContext &) -> TaskResult {
        assert(std::this_thread::get_id() != owner); return {TaskError::None, int64_t(42), {}};
    }, [&](const TaskResult &result) {
        assert(std::this_thread::get_id() == owner);
        assert(result.ok() && *std::get_if<int64_t>(&result.value) == 42); delivered = true;
    });
    until([&] { return delivered; }, &main);
    assert(number.status() == TaskStatus::Completed);
    std::atomic<bool> started{false};
    bool cancelled_delivered = false;
    auto running = tasks.submit([&](const TaskContext &context) -> TaskResult {
        started = true;
        while (!context.is_cancelled()) std::this_thread::yield();
        return {};
    }, [&](const TaskResult &) { cancelled_delivered = true; });
    until([&] { return started.load(); });
    assert(running.result().error == TaskError::NotReady);
    std::atomic<int> queued_ran{0};
    auto queued = tasks.submit([&](const TaskContext &) -> TaskResult { ++queued_ran; return {}; });
    assert(tasks.submit([](const TaskContext &) { return TaskResult{}; }).result().error == TaskError::QueueFull);
    assert(queued.cancel()); assert(running.cancel());
    until([&] { return running.status() == TaskStatus::Cancelled; });
    assert(running.is_cancel_requested() && queued.status() == TaskStatus::Cancelled);
    main.update(128, 0); assert(!cancelled_delivered && queued_ran == 0);
    auto wrong_shutdown = tasks.submit([&](const TaskContext &) -> TaskResult { assert(!tasks.shutdown()); return {}; });
    until([&] { return wrong_shutdown.status() == TaskStatus::Completed; });
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    auto failure = tasks.submit([](const TaskContext &) -> TaskResult { throw std::runtime_error("work failed"); });
    until([&] { return failure.status() == TaskStatus::Failed; });
    assert(failure.result().message == "work failed");
#endif
    auto waiting = tasks.submit([](const TaskContext &) { return TaskResult{}; }, [&](const TaskResult &) { assert(false); });
    until([&] { return main.pending_count() != 0; });
    main.shutdown();
    until([&] { return waiting.status() == TaskStatus::Cancelled; });
    auto blocked = tasks.submit([](const TaskContext &) { return TaskResult{}; }, [](const TaskResult &) {});
    until([&] { return blocked.status() == TaskStatus::Rejected; });
    assert(blocked.result().error == TaskError::Closed);
    assert(tasks.shutdown() && tasks.shutdown());
    assert(tasks.submit([](const TaskContext &) { return TaskResult{}; }).result().error == TaskError::Closed);
    MainThreadCenter last_main;
    TaskCenter last(last_main, 1);
    started = false;
    auto interrupted = last.submit([&](const TaskContext &context) -> TaskResult {
        started = true; while (!context.is_cancelled()) std::this_thread::yield(); return {};
    });
    until([&] { return started.load(); });
    assert(last.shutdown()); assert(interrupted.status() == TaskStatus::Cancelled);
}
}
int main() { dispatch_tests(); task_tests(); std::cout << "Async centers: PASS\n"; }
