#pragma once
#include <list>
#include <map>
#include <godot_cpp/classes/resource.hpp>

namespace meowody
{
// Only unpinned resources live here; group ownership is maintained separately.
class ResourceCache
{
public:
    explicit ResourceCache(std::size_t capacity = 128) : m_capacity(capacity) {}
    ResourceCache(const ResourceCache&) = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;
    godot::Ref<godot::Resource> get(const godot::String& key);
    void put(const godot::String& key, const godot::Ref<godot::Resource>& resource);
    void erase(const godot::String& key);
    void clear();
    void set_capacity(std::size_t capacity);
    std::size_t capacity() const { return m_capacity; }
    std::size_t size() const { return m_entries.size(); }
    std::uint64_t evictions() const { return m_evictions; }
private:
    struct Entry { godot::Ref<godot::Resource> resource; std::list<godot::String>::iterator order; };
    void trim();
    std::size_t m_capacity;
    std::uint64_t m_evictions = 0;
    std::list<godot::String> m_order;
    std::map<godot::String, Entry> m_entries;
};
}
