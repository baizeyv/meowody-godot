#include "resource_center.h"
#include "runtime/download/http_url.h"
#include <algorithm>
namespace meowody {
godot::String ResourceCenter::normalize(const godot::String& input)
{
    if (input.get_slice(":", 0).to_lower() == "http" || input.get_slice(":", 0).to_lower() == "https")
        return detail::parse_http_url(input).url;
    auto path = input.replace("\\", "/");
    if (path.is_empty()) return {};
    if (path.begins_with("uid://")) path = m_backend->resolve_uid(path);
    if (path.is_empty()) return {};
    if (!path.begins_with("res://") && !path.begins_with("user://"))
    {
        if (path.is_absolute_path() || path.contains("://")) return {};
        path = "res://" + path;
    }
    path = path.simplify_path();
    if ((!path.begins_with("res://") && !path.begins_with("user://")) ||
        path == "res://" || path == "user://" || path.contains("/../") || path.ends_with("/..")) return {};
    return path;
}

godot::Ref<godot::Resource> ResourceCenter::cached(const godot::String& path)
{
    for (const auto& [name, group] : m_groups)
    {
        const auto entry = group.resources.find(path);
        if (entry != group.resources.end()) return entry->second;
    }
    return m_cache.get(path);
}

godot::String ResourceCenter::pack_key(const godot::String& url, const godot::String& entry)
{
    const auto parsed = detail::parse_http_url(url);
    const auto path = normalize(entry);
    if (!parsed.valid() || !entry.begins_with("res://") || !path.begins_with("res://")) return {};
    const auto ext = detail::url_extension(parsed.url);
    if (ext != "pck" && ext != "zip") return {};
    return "pack:" + parsed.url.sha256_text() + godot::String(":") + path;
}
void ResourceCenter::remember(const godot::String& path, const godot::Ref<godot::Resource>& resource)
{
    for (const auto& [name, group] : m_groups)
        if (group.resources.contains(path)) return;
    m_cache.put(path, resource);
}

}
