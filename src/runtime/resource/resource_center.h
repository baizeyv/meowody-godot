#pragma once

#include "batch_load_handle.h"
#include "load_progress.h"
#include "resource_backend.h"
#include "resource_cache.h"
#include "remote_resource_loader.h"
#include "detail/batch_state.h"
#include "detail/resource_job.h"
#include "runtime/download/download_center.h"

namespace meowody
{
class ResourceCenter
{
public:
    ResourceCenter();
    explicit ResourceCenter(std::unique_ptr<detail::ResourceBackend> backend);
    explicit ResourceCenter(DownloadCenter& downloads);
    ~ResourceCenter();
    ResourceCenter(const ResourceCenter&) = delete;
    ResourceCenter& operator=(const ResourceCenter&) = delete;

    template <typename T = godot::Resource> requires std::derived_from<T, godot::Resource>
    LoadResult<T> load(const godot::String& path)
    {
        auto result = detail::typed_result<T>(load_raw(path));
        m_history.remember(normalize(path), {true, result ? LoadStatus::Succeeded : LoadStatus::Failed, result.error, 1.0f});
        return result;
    }

    template <typename T = godot::Resource> requires std::derived_from<T, godot::Resource>
    LoadHandle<T> load_async(const godot::String& path,
                           std::type_identity_t<Delegate<void(const LoadResult<T>&)>> callback = {})
    {
        return make_request<T>(path, {}, std::move(callback));
    }

    template <typename T = godot::Resource> requires std::derived_from<T, godot::Resource>
    LoadHandle<T> load_pack_async(const godot::String& pack_url, const godot::String& entry,
                                std::type_identity_t<Delegate<void(const LoadResult<T>&)>> callback = {})
    {
        return make_request<T>(pack_url, entry, std::move(callback));
    }

    BatchLoadHandle preload_group(const godot::String& group, const std::vector<godot::String>& paths,
                                 Delegate<void(const BatchLoadHandle&)> callback = {});
    BatchLoadHandle preload_pack_group(const godot::String& group, const godot::String& pack_url,
                                      const std::vector<godot::String>& entries,
                                      Delegate<void(const BatchLoadHandle&)> callback = {});
    template <typename T = godot::Resource> requires std::derived_from<T, godot::Resource>
    godot::Ref<T> get_cached(const godot::String& path) { return godot::Ref<T>(cached(normalize(path))); }
    template <typename T = godot::Resource> requires std::derived_from<T, godot::Resource>
    godot::Ref<T> get_cached_pack(const godot::String& url, const godot::String& entry)
    {
        return godot::Ref<T>(cached(pack_key(url, entry)));
    }
    void set_lru_capacity(std::size_t capacity) { m_cache.set_capacity(capacity); }
    std::size_t lru_capacity() const { return m_cache.capacity(); }
    std::size_t lru_size() const { return m_cache.size(); }
    std::uint64_t lru_evictions() const { return m_cache.evictions(); }
    void clear_lru() { m_cache.clear(); }
    LoadProgress query(const godot::String& path) { return m_history.query(normalize(path)); }
    LoadProgress query_pack(const godot::String& url, const godot::String& entry) { return m_history.query(pack_key(url, entry)); }
    LoadProgress query_group(const godot::String& group) const { return m_batch_history.query(group); }
    float progress_percent(const godot::String& path) { return query(path).progress_percent(); }
    bool is_finished(const godot::String& path) { return query(path).finished(); }
    bool has_cached(const godot::String& path) { return cached(normalize(path)).is_valid(); }
    bool evict_cached(const godot::String& path);
    bool has_group(const godot::String& group) const { return m_groups.contains(group); }
    std::size_t group_size(const godot::String& group) const;
    bool cancel(const godot::String& path);
    bool cancel_pack(const godot::String& url, const godot::String& entry);
    void cancel_all();
    std::size_t pending_count() const;
    bool is_idle() const { return m_jobs.empty() && m_requests.empty() && m_batches.empty(); }
    bool is_closed() const { return m_closed; }
    void release_group(const godot::String& group);
    void clear_cache();
    void update();
    void shutdown();

private:
    template <typename T>
    LoadHandle<T> make_request(const godot::String& path, const godot::String& entry,
                              Delegate<void(const LoadResult<T>&)> callback)
    {
        auto state = std::make_shared<detail::LoadState>();
        state->accepts = [](const godot::Ref<godot::Resource>& resource) { return godot::Ref<T>(resource).is_valid(); };
        if (callback)
            state->callback = [weak = std::weak_ptr(state), fn = std::move(callback)]() mutable {
                if (auto locked = weak.lock()) fn(detail::typed_result<T>(locked->result));
            };
        enqueue(path, state, entry);
        return LoadHandle<T>(std::move(state));
    }
    struct Group
    {
        std::uint64_t generation = 0;
        std::map<godot::String, godot::Ref<godot::Resource>> resources;
    };
    godot::String normalize(const godot::String& path);
    godot::String pack_key(const godot::String& url, const godot::String& entry);
    godot::Ref<godot::Resource> cached(const godot::String& path);
    void remember(const godot::String& path, const godot::Ref<godot::Resource>& resource);
    LoadResult<> load_raw(const godot::String& path);
    void enqueue(const godot::String& path, const std::shared_ptr<detail::LoadState>& state,
                 const godot::String& pack_entry = {});
    static void complete(detail::LoadState& state, const LoadResult<>& result);
    void finish_job(const godot::String& path, const LoadResult<>& result);
    void poll_jobs();
    void submit_local(const godot::String& key);
    void advance_remote(const godot::String& key);
    void advance_batch(const std::shared_ptr<detail::BatchState>& batch);
    bool cancel_key(const godot::String& key);
    BatchLoadHandle create_batch(const godot::String& group, const std::vector<godot::String>& paths,
                                const godot::String& pack_url, Delegate<void(const BatchLoadHandle&)> callback, bool is_pack = false);

    std::unique_ptr<detail::ResourceBackend> m_backend;
    std::unique_ptr<DownloadCenter> m_owned_downloads;
    DownloadCenter* m_downloads = nullptr;
    detail::RemoteResourceLoader m_remote;
    ResourceCache m_cache;
    std::map<godot::String, detail::ResourceJob> m_jobs;
    std::vector<godot::String> m_poll_keys;
    std::map<godot::String, Group> m_groups;
    std::vector<std::shared_ptr<detail::LoadState>> m_requests;
    std::vector<std::shared_ptr<detail::BatchState>> m_batches;
    std::uint64_t m_next_generation = 1;
    detail::RequestHistory<detail::LoadState, LoadProgress> m_history{LoadProgress::snapshot};
    detail::RequestHistory<detail::BatchState, LoadProgress> m_batch_history{LoadProgress::batch_snapshot};
    bool m_updating = false;
    bool m_closed = false;
};
}
