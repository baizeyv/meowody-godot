#include "resource_cache.h"

namespace meowody
{
godot::Ref<godot::Resource> ResourceCache::get(const godot::String& key)
{
    const auto entry = m_entries.find(key);
    if (entry == m_entries.end()) return {};
    m_order.splice(m_order.begin(), m_order, entry->second.order);
    return entry->second.resource;
}
void ResourceCache::put(const godot::String& key, const godot::Ref<godot::Resource>& resource)
{
    if (m_capacity == 0 || resource.is_null()) return;
    const auto found = m_entries.find(key);
    if (found != m_entries.end())
    {
        found->second.resource = resource;
        m_order.splice(m_order.begin(), m_order, found->second.order);
    }
    else
    {
        m_order.push_front(key);
        m_entries.emplace(key, Entry{resource, m_order.begin()});
    }
    trim();
}
void ResourceCache::erase(const godot::String& key)
{
    const auto it = m_entries.find(key);
    if (it == m_entries.end()) return;
    m_order.erase(it->second.order);
    m_entries.erase(it);
}
void ResourceCache::clear() { m_entries.clear(); m_order.clear(); }
void ResourceCache::set_capacity(std::size_t capacity) { m_capacity = capacity; trim(); }
void ResourceCache::trim()
{
    while (m_entries.size() > m_capacity)
    {
        m_entries.erase(m_order.back());
        m_order.pop_back();
        ++m_evictions;
    }
}
}
