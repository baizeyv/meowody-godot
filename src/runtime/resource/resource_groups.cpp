#include "resource_center.h"
namespace meowody {
BatchLoadHandle ResourceCenter::preload_group(const godot::String& group, const std::vector<godot::String>& paths,
                                            Delegate<void(const BatchLoadHandle&)> callback)
{
    return create_batch(group, paths, {}, std::move(callback));
}
BatchLoadHandle ResourceCenter::preload_pack_group(const godot::String& group, const godot::String& pack_url,
                                                  const std::vector<godot::String>& entries,
                                                  Delegate<void(const BatchLoadHandle&)> callback)
{
    return create_batch(group, entries, pack_url, std::move(callback), true);
}
BatchLoadHandle ResourceCenter::create_batch(const godot::String& group, const std::vector<godot::String>& paths,
                                            const godot::String& pack_url, Delegate<void(const BatchLoadHandle&)> callback, bool is_pack)
{
    auto batch = std::make_shared<detail::BatchState>();
    batch->group = group;
    if (m_closed)
    {
        for (const auto& path : paths) batch->items.push_back(load_async<>(path));
        batch->status = LoadStatus::Cancelled;
        return BatchLoadHandle(batch);
    }
    auto [entry, inserted] = m_groups.try_emplace(group);
    if (inserted) entry->second.generation = m_next_generation++;
    batch->generation = entry->second.generation;
    for (const auto& path : paths)
    {
        batch->paths.push_back(is_pack ? pack_key(pack_url, path) : normalize(path));
        batch->items.push_back(is_pack ? load_pack_async<>(pack_url, path) : load_async<>(path));
    }
    if (callback)
        batch->callback = [weak = std::weak_ptr(batch), fn = std::move(callback)]() mutable {
            if (auto locked = weak.lock()) fn(BatchLoadHandle(locked));
        };
    m_batches.push_back(batch);
    m_batch_history.track(group, batch);
    return BatchLoadHandle(batch);
}

void ResourceCenter::advance_batch(const std::shared_ptr<detail::BatchState>& batch)
{
    if (is_load_finished(batch->status)) return;
    const auto group = m_groups.find(batch->group);
    if (group == m_groups.end() || group->second.generation != batch->generation)
    {
        BatchLoadHandle(batch).cancel();
        return;
    }
    bool done = true;
    bool failed = false;
    for (std::size_t i = 0; i < batch->items.size(); ++i)
    {
        const auto& item = batch->items[i];
        done &= item.finished();
        failed |= item.status() == LoadStatus::Failed || item.status() == LoadStatus::Cancelled;
        if (item.status() == LoadStatus::Succeeded)
        {
            group->second.resources.emplace(batch->paths[i], item.result().resource);
            m_cache.erase(batch->paths[i]);
        }
    }
    batch->status = done ? (failed ? LoadStatus::Failed : LoadStatus::Succeeded) : LoadStatus::Loading;
}

void ResourceCenter::release_group(const godot::String& group)
{
    for (const auto& batch : m_batches)
        if (batch->group == group) BatchLoadHandle(batch).cancel();
    m_groups.erase(group);
}
void ResourceCenter::clear_cache()
{
    for (const auto& batch : m_batches) BatchLoadHandle(batch).cancel();
    m_groups.clear();
    m_cache.clear();
}

}
