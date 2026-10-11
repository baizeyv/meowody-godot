#include "pool_center.h"
#include "detail/node_pool.h"

namespace meowody {
using namespace godot;
using namespace detail;
Node* PoolCenter::acquire(const String& name, Node* parent) {
    if (m_closed || m_busy || (parent && parent->is_queued_for_deletion())) return nullptr;
    auto it = m_pools.find(name); if (it == m_pools.end()) return nullptr;
    uint64_t parent_id = parent ? parent->get_instance_id() : 0;
    PoolGuard guard(m_busy); auto& p = *it->second; prune(p);
    Node* node = nullptr;
    if (!p.idle.empty()) {
        uint64_t id = p.idle.back(); p.idle.pop_back(); node = live_node(id);
        p.nodes[id].idle = false;
    } else node = instantiate(p);
    if (!node) return nullptr;
    uint64_t id = node->get_instance_id();
    if (parent_id) {
        parent = live_node(parent_id);
        if (!parent) { p.nodes.erase(id); destroy_node(id); return nullptr; }
    }
    node->set_process_mode(p.nodes[id].process_mode);
    if (parent) parent->add_child(node);
    node = live_node(id);
    if (node && p.acquire_hook) p.acquire_hook(node);
    node = live_node(id);
    if (!node) p.nodes.erase(id);
    return node;
}
bool PoolCenter::release(Node* node) {
    if (m_closed || m_busy || !node) return false;
    uint64_t id = node->get_instance_id();
    for (auto& [name, pool] : m_pools) {
        auto& p = *pool; auto entry = p.nodes.find(id);
        if (entry == p.nodes.end()) continue;
        if (entry->second.idle || !live_node(id)) return false;
        PoolGuard guard(m_busy);
        if (p.release_hook) p.release_hook(node);
        node = live_node(id);
        if (node && node->get_parent()) node->get_parent()->remove_child(node);
        node = live_node(id);
        if (!node) { p.nodes.erase(id); return true; }
        prune(p);
        if (p.idle.size() >= p.options.max_idle || (p.options.max_total && p.nodes.size() > p.options.max_total)) {
            p.nodes.erase(id); destroy_node(id); return true;
        }
        node->set_process_mode(Node::PROCESS_MODE_DISABLED);
        p.nodes[id].idle = true; p.idle.push_back(id); return true;
    }
    return false;
}
bool PoolCenter::discard(Node* node) {
    if (m_closed || m_busy || !node) return false;
    uint64_t id = node->get_instance_id();
    for (auto& [name, pool] : m_pools) {
        if (!pool->nodes.erase(id)) continue;
        PoolGuard guard(m_busy); prune(*pool); destroy_node(id); return true;
    }
    return false;
}
PoolStats PoolCenter::stats(const String& name) const {
    auto it = m_pools.find(name); if (it == m_pools.end()) return {};
    PoolStats result; result.exists = true; result.max_idle = it->second->options.max_idle; result.max_total = it->second->options.max_total;
    for (auto [id, entry] : it->second->nodes) if (live_node(id)) { if (entry.idle) ++result.idle; else ++result.active; }
    return result;
}
bool PoolCenter::contains(const String& name) const { return m_pools.contains(name); }
bool PoolCenter::owns(Node* node) const {
    if (!node) return false;
    auto id = node->get_instance_id();
    for (auto& [name, p] : m_pools) if (p->nodes.contains(id) && live_node(id)) return true;
    return false;
}
bool PoolCenter::is_idle(Node* node) const {
    if (!node) return false;
    auto id = node->get_instance_id();
    for (auto& [name, p] : m_pools) { auto it = p->nodes.find(id); if (it != p->nodes.end()) return it->second.idle && live_node(id); }
    return false;
}
PackedStringArray PoolCenter::names() const {
    PackedStringArray result; for (auto& [name, p] : m_pools) result.append(name); return result;
}
}
