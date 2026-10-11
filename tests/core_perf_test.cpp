#include "runtime/event/event_channel.hpp"
#include "runtime/coroutine/coroutine_center.h"
#include "runtime/reactive/computed.hpp"
#include "runtime/reactive/effect.hpp"
#include "runtime/reactive/reactive_value.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
std::size_t allocations = 0;
std::uint64_t checksum = 0;
struct Event { int value = 1; };

void event_mutation_test() {
    meowody::EventChannel<Event> channel;
    std::vector<int> delivered;
    meowody::Connection pending, replacement, added;
    bool first = true;
    auto mutator = channel.connect([&](const Event&) {
        delivered.push_back(1);
        if (!first) return;
        first = false;
        pending.disconnect();
        replacement = channel.connect([&] { delivered.push_back(3); });
        added = channel.connect([&] { delivered.push_back(4); });
    });
    pending = channel.connect([&] { delivered.push_back(2); });
    channel.emit();
    assert((delivered == std::vector<int>{1}));
    channel.emit();
    assert((delivered == std::vector<int>{1, 1, 3, 4}));
    channel.clear();
    auto after_clear = channel.connect([&] { delivered.push_back(5); });
    mutator.disconnect(); replacement.disconnect(); added.disconnect();
    channel.emit();
    assert(delivered.back() == 5); // Stale connections cannot remove reused slots.

    meowody::EventChannel<Event> nested;
    delivered.clear();
    meowody::Connection nested_added;
    auto a = nested.connect([&](const Event& e) {
        delivered.push_back(10 + e.value);
        if (e.value == 1) {
            nested_added = nested.connect([&](const Event& inner) { delivered.push_back(30 + inner.value); });
            nested.emit({2});
        }
    });
    auto b = nested.connect([&](const Event& e) { delivered.push_back(20 + e.value); });
    nested.emit({1});
    assert((delivered == std::vector<int>{11, 12, 22, 32, 21}));

    auto destroyed = std::make_unique<meowody::EventChannel<Event>>();
    auto destroyer = destroyed->connect([&] { destroyed.reset(); });
    auto suppressed = destroyed->connect([&] { assert(false); });
    destroyed->emit(); // State/callable must survive destruction during delivery.
    assert(!destroyer.connected() && !suppressed.connected());

    meowody::EventChannel<Event> growing;
    auto payload = std::make_shared<int>(7);
    std::weak_ptr<int> alive = payload;
    meowody::Connection self;
    std::vector<meowody::Connection> growth;
    self = growing.connect([&, held = payload] {
        self.disconnect();
        for (int i = 0; i < 128; ++i) growth.push_back(growing.connect([] {}));
        assert(!alive.expired() && *held == 7);
    });
    payload.reset();
    growing.emit();
    assert(alive.expired()); // Self-removal and vector growth never destroy the running callback.
}

void coroutine_mutation_test() {
    meowody::CoroutineCenter center;
    std::vector<int> delivered;
    meowody::CoroutineCenter::CoroutineId pending = 0, replacement = 0;
    auto a = meowody::Coroutine::make();
    a->then([&] {
        delivered.push_back(1);
        assert(center.stop(pending));
        auto b = meowody::Coroutine::make();
        b->then([&] { delivered.push_back(3); });
        replacement = center.start(b);
        auto c = meowody::Coroutine::make();
        c->then([&] { delivered.push_back(4); });
        static_cast<void>(center.start(c));
        center.update(1); // Reentrant update is ignored.
    });
    static_cast<void>(center.start(a));
    auto b = meowody::Coroutine::make();
    b->then([&] { delivered.push_back(2); });
    pending = center.start(b);
    center.update(1);
    assert((delivered == std::vector<int>{1}));
    assert(!center.stop(pending) && center.active_count() == 2);
    center.update(1);
    assert((delivered == std::vector<int>{1, 3, 4}) && center.empty());
    assert(!center.stop(replacement));

    auto clearer = meowody::Coroutine::make();
    clearer->then([&] {
        center.clear();
        auto fresh = meowody::Coroutine::make();
        fresh->then([&] { delivered.push_back(5); });
        static_cast<void>(center.start(fresh));
    });
    static_cast<void>(center.start(clearer));
    center.update(1);
    assert(delivered.back() == 4 && center.active_count() == 1);
    center.update(1);
    assert(delivered.back() == 5 && center.empty());
}

void dependency_test() {
    meowody::Reactive<int> selector{0}, left{2}, right{7};
    std::vector<int> delivered;
    auto derived = meowody::computed([&] { return selector.get() ? right.get() : left.get(); });
    auto effect = std::make_unique<meowody::Effect>([&] { delivered.push_back(derived.get()); });
    left.set(3); selector.set(1); left.set(4); right.set(8);
    assert((delivered == std::vector<int>{2, 3, 7, 8}));
    effect.reset(); right.set(9);
    assert(delivered.size() == 4);
    auto source = std::make_unique<meowody::Reactive<int>>(10);
    auto surviving = std::make_unique<meowody::Effect>([&] { checksum += source->get(); });
    source.reset(); surviving.reset(); // Reciprocal unlink also works when source dies first.
}

#ifndef MEOWODY_BASELINE
void subscriber_mutation_test() {
    meowody::Reactive<int> source;
    auto payload = std::make_shared<int>(7);
    std::weak_ptr<int> alive = payload;
    meowody::Connection self;
    std::vector<meowody::Connection> growth;
    self = source.subscribe([&, held = payload](const int&) {
        self.disconnect();
        for (int i = 0; i < 128; ++i) growth.push_back(source.subscribe([](const int&) {}));
        assert(!alive.expired() && *held == 7);
    });
    payload.reset(); source.set(1);
    assert(alive.expired());

    meowody::Reactive<int> nested;
    std::vector<int> delivered;
    meowody::Connection pending, replacement, added;
    auto mutator = nested.subscribe([&](const int& value) {
        delivered.push_back(10 + value);
        if (value == 1) {
            pending.disconnect();
            replacement = nested.subscribe([&](const int& v) { delivered.push_back(20 + v); });
            added = nested.subscribe([&](const int& v) { delivered.push_back(30 + v); });
            nested.set(2);
            assert(value == 2); // Reentrant set still updates the same referenced value.
        }
    });
    pending = nested.subscribe([&](const int& v) { delivered.push_back(40 + v); });
    nested.set(1);
    assert((delivered == std::vector<int>{11, 12, 22, 32}));
    nested.set(3);
    assert((delivered == std::vector<int>{11, 12, 22, 32, 13, 23, 33}));

    meowody::Reactive<int> recursive;
    payload = std::make_shared<int>(8); alive = payload;
    meowody::Connection recursive_self;
    recursive_self = recursive.subscribe([&, held = payload](const int& value) {
        if (value == 1) recursive.set(2);
        else recursive_self.disconnect();
        assert(!alive.expired() && *held == 8);
    });
    payload.reset(); recursive.set(1);
    assert(alive.expired());

#if defined(__cpp_exceptions)
    meowody::Reactive<int> throwing;
    payload = std::make_shared<int>(9); alive = payload;
    meowody::Connection throwing_self;
    throwing_self = throwing.subscribe([&, held = payload](const int&) {
        throwing_self.disconnect();
        assert(*held == 9);
        throw 1;
    });
    payload.reset();
    try { throwing.set(1); assert(false); } catch (int) {}
    assert(alive.expired());
    auto reused = throwing.subscribe([&](const int&) { ++checksum; });
    throwing.set(2); // The exception unwinds the invocation guard and frees the slot.
#endif
}
#endif

template<class F>
void benchmark(const char* name, int iterations, F&& operation) {
    operation(); // Warm up capacities and lazy code paths.
    const auto before = allocations;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) operation();
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const auto count = allocations - before;
    std::cout << name << " iterations=" << iterations << " allocations=" << count << " ms=" << elapsed << '\n';
}
}

void* operator new(std::size_t size) {
    ++allocations;
    if (auto* memory = std::malloc(size ? size : 1)) return memory;
    std::abort();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    event_mutation_test(); coroutine_mutation_test(); dependency_test();
#ifndef MEOWODY_BASELINE
    subscriber_mutation_test(); // The original callback storage is unsafe under these mutations.
#endif
    meowody::EventChannel<Event> channel;
    std::vector<meowody::Connection> subscribers;
    for (int i = 0; i < 16; ++i)
        subscribers.push_back(channel.connect([](const Event& event) { checksum += event.value; }));
    benchmark("event_16", 200000, [&] { channel.emit(); });

    meowody::CoroutineCenter center;
    for (int i = 0; i < 256; ++i) {
        auto coroutine = meowody::Coroutine::make();
        coroutine->push_step([](double) { ++checksum; return false; });
        static_cast<void>(center.start(coroutine));
    }
    benchmark("coroutine_256", 30000, [&] { center.update(1.0 / 60); });

    meowody::Reactive<int> source;
    std::vector<std::unique_ptr<meowody::Effect>> effects;
    for (int i = 0; i < 16; ++i)
        effects.push_back(std::make_unique<meowody::Effect>([&] { checksum += source.get(); }));
    int next = 0;
    benchmark("reactive_16_effects", 100000, [&] { source.set(++next); });

    meowody::Reactive<int> observed;
    std::vector<meowody::Connection> subscriptions;
    for (int i = 0; i < 16; ++i)
        subscriptions.push_back(observed.subscribe([](const int& value) { checksum += value; }));
    int next_observed = 0;
    benchmark("reactive_16_subscribers", 1000000, [&] { observed.set(++next_observed); });
    benchmark("reactive_register_16", 10000, [] {
        meowody::Reactive<int> value;
        std::vector<meowody::Connection> connections;
        connections.reserve(16);
        for (int i = 0; i < 16; ++i)
            connections.push_back(value.subscribe([](const int&) {}));
    });
    std::cout << "Core performance regressions: PASS checksum=" << checksum << '\n';
}
