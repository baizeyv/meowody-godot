#include "runtime/system/system_center.h"
#include <cassert>
#include <vector>

namespace {
std::vector<int> destroyed;
struct First {
    int value;
    explicit First(int v = 1) : value(v) {}
    ~First() { destroyed.push_back(1); }
};
struct Second {
    ~Second() { destroyed.push_back(2); }
};
struct Dependent {
    std::shared_ptr<First> first;
    explicit Dependent(std::shared_ptr<First> p) : first(std::move(p)) {}
};
std::vector<int> lifecycle;
struct ServiceA : meowody::System {
    bool active = false;
    meowody::SystemStatus initialize() override {
        lifecycle.push_back(1); active = true; return {};
    }
    void shutdown() noexcept override {
        lifecycle.push_back(-1); active = false;
    }
};
struct ServiceB : meowody::System {
    meowody::SystemCenter &center;
    bool fail = true;
    explicit ServiceB(meowody::SystemCenter &p) : center(p) {}
    meowody::SystemStatus initialize() override {
        lifecycle.push_back(2);
        assert(center.get<ServiceA>()->active);
        assert(center.get<Second>()); // All registrations are available before startup.
        assert(!center.register_system<First>());
        assert(center.initialize().error == meowody::SystemError::InvalidState);
        center.clear(); center.shutdown(); // Reentrant lifecycle mutation is ignored.
        assert(center.get<ServiceA>());
        if (fail) return {meowody::SystemError::InitializationFailed, "ServiceB failed"};
        return {};
    }
    void shutdown() noexcept override {
        lifecycle.push_back(-2);
        assert(center.get<ServiceA>()->active);
        assert(!center.register_system<First>());
        center.clear(); center.shutdown();
    }
};
struct ServiceC : meowody::System {
    meowody::SystemStatus initialize() override { lifecycle.push_back(3); return {}; }
    void shutdown() noexcept override { lifecycle.push_back(-3); }
};
void lifecycle_test() {
    meowody::SystemCenter center;
    auto a = center.register_system<ServiceA>();
    auto b = center.register_system<ServiceB>(center);
    assert((center.register_systems<ServiceC, Second>()));
    auto result = center.initialize();
    assert(result.error == meowody::SystemError::InitializationFailed && result.message == "ServiceB failed");
    assert((lifecycle == std::vector<int>{1, 2, -2, -1}));
    assert(!center.is_initialized() && !a->active);
    b->fail = false; lifecycle.clear();
    assert(center.initialize().ok() && center.is_initialized());
    assert((lifecycle == std::vector<int>{1, 2, 3}));
    assert(center.initialize().ok()); // No duplicate startup.
    assert(center.register_system<ServiceA>() == a);
    assert(!center.register_system<First>());
    assert(!center.register_systems<First>());
    lifecycle.clear(); center.shutdown();
    assert((lifecycle == std::vector<int>{-3, -2, -1}));
    assert(!a->active && !center.is_initialized());
    assert(center.get<ServiceA>() == a); // Stop does not release ownership.
    center.shutdown();
    assert(lifecycle.size() == 3);
    assert(center.initialize().error == meowody::SystemError::InvalidState);
    assert(!center.register_system<First>());
    center.clear();
    assert(!center.get<ServiceA>() && !a->active);
    assert(lifecycle.size() == 3);
    assert(center.register_system<First>()); // New registration cycle after clear.
    lifecycle.clear();
    std::shared_ptr<ServiceA> retained;
    {
        meowody::SystemCenter local;
        retained = local.register_system<ServiceA>();
        assert(local.initialize().ok());
    }
    assert(retained && !retained->active);
    assert((lifecycle == std::vector<int>{1, -1}));
    meowody::SystemCenter empty;
    assert(empty.initialize().ok());
    empty.clear();
    assert(!empty.is_initialized());
}
}
int main() {
    lifecycle_test();
    destroyed.clear();
    meowody::SystemCenter systems;
    assert(!systems.get<First>());
    auto first = systems.register_system<First>(7);
    assert(systems.register_system<First>(9) == first);
    assert(systems.get<First>()->value == 7);
    systems.register_systems<First, Second>();
    auto dependent = systems.register_system<Dependent>(first);
    assert(dependent->first == first);
    dependent.reset();
    first.reset();
    systems.clear();
    assert((destroyed == std::vector<int>{2, 1}));
    assert(!systems.get<First>());
    systems.clear();
    systems.register_system<First>();
    assert(systems.get<First>()->value == 1);
}
