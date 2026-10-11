#include "resource_center.h"

namespace meowody
{
bool ResourceCenter::evict_cached(const godot::String& path)
{
    const auto key = normalize(path);
    for (const auto& [name, group] : m_groups)
        if (group.resources.contains(key)) return false;
    const bool found = m_cache.get(key).is_valid();
    m_cache.erase(key);
    return found;
}
std::size_t ResourceCenter::group_size(const godot::String& group) const
{
    const auto it = m_groups.find(group);
    return it == m_groups.end() ? 0 : it->second.resources.size();
}
bool ResourceCenter::cancel_key(const godot::String& key)
{
    bool found = false;
    for (const auto& state : m_requests)
        if (state->path == key) { state->cancel(); found = true; }
    return found;
}
bool ResourceCenter::cancel(const godot::String& path) { return cancel_key(normalize(path)); }
bool ResourceCenter::cancel_pack(const godot::String& url, const godot::String& entry) { return cancel_key(pack_key(url, entry)); }
void ResourceCenter::cancel_all()
{
    for (const auto& state : m_requests) state->cancel();
    for (const auto& batch : m_batches) BatchLoadHandle(batch).cancel();
}
std::size_t ResourceCenter::pending_count() const
{
    std::size_t count = 0;
    for (const auto& state : m_requests) count += !is_load_finished(state->status);
    return count;
}
}
