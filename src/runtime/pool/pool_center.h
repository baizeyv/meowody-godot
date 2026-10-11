#pragma once
#include "i_poolable.h"
#include "pool_types.h"
#include "runtime/core/delegate.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <map>
#include <memory>
#include <type_traits>

namespace meowody {
namespace detail { struct NodePool; }
class PoolCenter {
public:
    PoolCenter();
    ~PoolCenter();
    PoolCenter(const PoolCenter&) = delete;
    PoolCenter& operator=(const PoolCenter&) = delete;
    template<class T = godot::Node> bool create(const godot::String& name, PoolOptions options = {}) {
        static_assert(std::is_base_of_v<godot::Node, T>);
        return create_factory<T>(name, []() -> godot::Node* { return memnew(T); }, options);
    }
    template<class T = godot::Node> bool create_scene(const godot::String& name, const godot::Ref<godot::PackedScene>& scene,
            PoolOptions options = {}) {
        if (scene.is_null()) return false;
        return create_factory<T>(name, [scene]() -> godot::Node* {
            auto* node = scene->instantiate();
            if (node && !godot::Object::cast_to<T>(node)) { memdelete(node); return nullptr; }
            return node;
        }, options);
    }
    template<class T = godot::Node> bool create_factory(const godot::String& name, Delegate<godot::Node*()> factory,
            PoolOptions options = {}) {
        static_assert(std::is_base_of_v<godot::Node, T>);
        Delegate<void(godot::Node*)> acquire_hook, release_hook;
        if constexpr (std::is_base_of_v<IPoolable, T>) {
            acquire_hook = [](godot::Node* node) { if (auto* typed = godot::Object::cast_to<T>(node)) typed->on_pool_acquire(); };
            release_hook = [](godot::Node* node) { if (auto* typed = godot::Object::cast_to<T>(node)) typed->on_pool_release(); };
        }
        return register_pool(name, std::move(factory), options, std::move(acquire_hook), std::move(release_hook));
    }
    godot::Node* acquire(const godot::String& name, godot::Node* parent = nullptr);
    template<class T> T* acquire_as(const godot::String& name, godot::Node* parent = nullptr) {
        auto* node = acquire(name, parent);
        auto* typed = godot::Object::cast_to<T>(node);
        if (node && !typed) release(node);
        return typed;
    }
    bool release(godot::Node* node);
    bool discard(godot::Node* node); // Destroy a borrowed or idle node owned by this center.
    size_t prewarm(const godot::String& name, size_t target_idle);
    bool set_capacity(const godot::String& name, size_t max_idle, size_t max_total = 0);
    PoolStats stats(const godot::String& name) const;
    bool contains(const godot::String& name) const;
    bool owns(godot::Node* node) const;
    bool is_idle(godot::Node* node) const;
    godot::PackedStringArray names() const;
    bool clear_idle(const godot::String& name);
    // Removing a pool destroys idle nodes; active nodes become caller-owned unless destroy_active is true.
    bool remove(const godot::String& name, bool destroy_active = false);
    void shutdown(); // Destroys every still-live owned node, including active ones.
    bool is_closed() const { return m_closed; }
private:
    std::map<godot::String, std::unique_ptr<detail::NodePool>> m_pools;
    bool m_busy = false;
    bool m_closed = false;
    bool register_pool(const godot::String&, Delegate<godot::Node*()>, PoolOptions,
        Delegate<void(godot::Node*)>, Delegate<void(godot::Node*)>);
    size_t warm(detail::NodePool&, size_t);
    godot::Node* instantiate(detail::NodePool&);
};
}
