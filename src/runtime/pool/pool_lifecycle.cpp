#include "pool_center.h"
#include "detail/node_pool.h"
#include <godot_cpp/core/object.hpp>
#include <algorithm>

namespace meowody::detail {
godot::Node* live_node(uint64_t id) {
    auto* node = godot::Object::cast_to<godot::Node>(godot::ObjectDB::get_instance(id));
    return node && !node->is_queued_for_deletion() ? node : nullptr;
}
void prune(NodePool& pool) {
    for (auto it = pool.nodes.begin(); it != pool.nodes.end();)
        if (!live_node(it->first)) it = pool.nodes.erase(it); else ++it;
    std::erase_if(pool.idle, [&](uint64_t id) { auto it = pool.nodes.find(id); return it == pool.nodes.end() || !it->second.idle; });
}
void destroy_node(uint64_t id, bool immediate) {
    auto* node = live_node(id); if (!node) return;
    if (auto* parent = node->get_parent()) parent->remove_child(node);
    node = live_node(id); if (!node) return;
    if (immediate) memdelete(node); else node->queue_free();
}
}
namespace meowody {
using namespace godot;
using namespace detail;
PoolCenter::PoolCenter() = default;
PoolCenter::~PoolCenter() { shutdown(); }
bool PoolCenter::register_pool(const String& name, Delegate<Node*()> factory, PoolOptions options,
        Delegate<void(Node*)> acquire_hook, Delegate<void(Node*)> release_hook) {
    if (m_closed || m_busy || name.is_empty() || !factory || contains(name)) return false;
    PoolGuard guard(m_busy);
    auto pool = std::make_unique<NodePool>(); pool->options = options; pool->factory = std::move(factory);
    pool->acquire_hook = std::move(acquire_hook); pool->release_hook = std::move(release_hook);
    auto* raw = pool.get(); m_pools.emplace(name, std::move(pool));
    warm(*raw, options.prewarm); return true;
}
Node* PoolCenter::instantiate(NodePool& pool) {
    prune(pool);
    if (pool.options.max_total && pool.nodes.size() >= pool.options.max_total) return nullptr;
    auto* node = pool.factory();
    if (!node || node->is_queued_for_deletion() || node->get_parent() || owns(node)) return nullptr;
    pool.nodes.emplace(node->get_instance_id(), PoolEntry{false, node->get_process_mode()});
    return node;
}
size_t PoolCenter::warm(NodePool& pool, size_t target) {
    prune(pool); target = std::min(target, pool.options.max_idle);
    if (pool.options.max_total) target = std::min(target, pool.options.max_total - std::min(pool.nodes.size() - pool.idle.size(), pool.options.max_total));
    size_t created = 0;
    while (pool.idle.size() < target) {
        auto* node = instantiate(pool); if (!node) break;
        uint64_t id = node->get_instance_id();
        if (pool.release_hook) pool.release_hook(node);
        node = live_node(id);
        if (!node) { pool.nodes.erase(id); break; }
        if (auto* parent = node->get_parent()) parent->remove_child(node);
        node = live_node(id); if (!node) { pool.nodes.erase(id); break; }
        node->set_process_mode(Node::PROCESS_MODE_DISABLED);
        pool.nodes[id].idle = true; pool.idle.push_back(id); ++created;
    }
    return created;
}
size_t PoolCenter::prewarm(const String& name, size_t target_idle) {
    if (m_closed || m_busy) return 0;
    auto it = m_pools.find(name); if (it == m_pools.end()) return 0;
    PoolGuard guard(m_busy); return warm(*it->second, target_idle);
}
bool PoolCenter::set_capacity(const String& name, size_t max_idle, size_t max_total) {
    if (m_closed || m_busy) return false;
    auto it = m_pools.find(name); if (it == m_pools.end()) return false;
    PoolGuard guard(m_busy); auto& p = *it->second; prune(p);
    p.options.max_idle = max_idle; p.options.max_total = max_total;
    while (!p.idle.empty() && (p.idle.size() > max_idle || (max_total && p.nodes.size() > max_total))) {
        uint64_t id = p.idle.back(); p.idle.pop_back(); p.nodes.erase(id); destroy_node(id);
    }
    return true;
}
bool PoolCenter::clear_idle(const String& name) {
    if (m_closed || m_busy) return false;
    auto it = m_pools.find(name); if (it == m_pools.end()) return false;
    PoolGuard guard(m_busy); auto& p = *it->second;
    auto idle = std::move(p.idle); p.idle.clear();
    for (auto id : idle) { p.nodes.erase(id); destroy_node(id); }
    return true;
}
bool PoolCenter::remove(const String& name, bool destroy_active) {
    if (m_closed || m_busy) return false;
    auto it = m_pools.find(name); if (it == m_pools.end()) return false;
    PoolGuard guard(m_busy); auto p = std::move(it->second); m_pools.erase(it);
    for (auto [id, entry] : p->nodes) if (entry.idle || destroy_active) destroy_node(id);
    return true;
}
void PoolCenter::shutdown() {
    if (m_closed || m_busy) return;
    PoolGuard guard(m_busy); m_closed = true;
    auto pools = std::move(m_pools); m_pools.clear();
    for (auto& [name, p] : pools) for (auto [id, entry] : p->nodes) destroy_node(id, true);
}
}
