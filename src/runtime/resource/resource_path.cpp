#include "resource_center.h"
#include <algorithm>
namespace meowody {
godot::String ResourceCenter::normalize(const godot::String& address)
{
    return !address.is_empty() && !address.contains("://") ? address : godot::String();
}
godot::Ref<godot::Resource> ResourceCenter::cached(const godot::String& address)
{
    for (const auto& [name, group] : m_groups) {
        auto entry = group.resources.find(address);
        if (entry != group.resources.end()) return entry->second;
    }
    auto found = m_retained.find(address);
    if (found != m_retained.end()) {
        std::erase_if(found->second, [](const auto& state) { return state.expired(); });
        for (const auto& weak : found->second) if (auto state = weak.lock(); state && state->result) return state->result.resource;
        if (found->second.empty()) m_retained.erase(found);
    }
    return m_cache.get(address);
}
void ResourceCenter::remember(const godot::String& address, const godot::Ref<godot::Resource>& resource)
{
    m_cache.put(address, resource);
}
}
