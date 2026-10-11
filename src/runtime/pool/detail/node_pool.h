#pragma once
#include "runtime/pool/pool_types.h"
#include "runtime/core/delegate.hpp"
#include <godot_cpp/classes/node.hpp>
#include <map>
#include <vector>
namespace meowody::detail {
struct PoolEntry {
    bool idle = false;
    godot::Node::ProcessMode process_mode = godot::Node::PROCESS_MODE_INHERIT;
};
struct NodePool {
    PoolOptions options;
    Delegate<godot::Node*()> factory;
    Delegate<void(godot::Node*)> acquire_hook;
    Delegate<void(godot::Node*)> release_hook;
    std::map<uint64_t, PoolEntry> nodes;
    std::vector<uint64_t> idle;
};
godot::Node* live_node(uint64_t id);
void prune(NodePool& pool);
void destroy_node(uint64_t id, bool immediate = false);
struct PoolGuard { bool& busy; PoolGuard(bool& flag) : busy(flag) { busy = true; } ~PoolGuard() { busy = false; } };
}
