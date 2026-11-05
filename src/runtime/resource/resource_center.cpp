#include "resource_center.h"
#include <algorithm>
#include <cassert>

namespace meowody
{
ResourceCenter::ResourceCenter() : ResourceCenter(detail::make_resource_backend()) {}
ResourceCenter::ResourceCenter(std::unique_ptr<detail::ResourceBackend> backend)
    : m_backend(std::move(backend)), m_owned_downloads(std::make_unique<DownloadCenter>()),
      m_downloads(m_owned_downloads.get()), m_catalog(std::make_unique<detail::ResourceCatalog>(*m_downloads)) { assert(m_backend); m_cache.set_capacity(0); }
ResourceCenter::ResourceCenter(DownloadCenter& downloads)
    : m_backend(detail::make_resource_backend()), m_downloads(&downloads), m_catalog(std::make_unique<detail::ResourceCatalog>(downloads)) { m_cache.set_capacity(0); }
ResourceCenter::~ResourceCenter() { shutdown(); }
void ResourceCenter::set_suspended(bool suspended) {
    if (m_closed) return;
    m_suspended = suspended;
    if (m_owned_downloads) m_owned_downloads->set_suspended(suspended);
}

void ResourceCenter::update()
{
    if (m_closed || m_updating || m_suspended || is_idle()) return;
    m_updating = true;
    struct Guard { bool& flag; ~Guard() { flag = false; } } guard{m_updating};
    if (m_owned_downloads) m_owned_downloads->update();
    m_catalog->update();
    for (auto it = m_retained.begin(); it != m_retained.end(); ) {
        std::erase_if(it->second, [](const auto& state) { return state.expired(); });
        if (it->second.empty()) it = m_retained.erase(it); else ++it;
    }
    poll_jobs();
    for (const auto& batch : m_batches) advance_batch(batch);
    m_history.refresh();
    m_batch_history.refresh();

    std::vector<std::shared_ptr<detail::LoadState>> ready;
    std::vector<std::shared_ptr<detail::BatchState>> batches;
    std::erase_if(m_requests, [&](const auto& request) {
        if (!is_load_finished(request->status)) return false;
        ready.push_back(request);
        return true;
    });
    std::erase_if(m_batches, [&](const auto& batch) {
        if (!is_load_finished(batch->status)) return false;
        batches.push_back(batch);
        return true;
    });
    for (const auto& request : ready)
    {
        if (m_suspended && !m_closed) { m_requests.push_back(request); continue; }
        auto callback = std::move(request->callback);
        if (!m_closed && !request->suppress_callback && callback) callback();
    }
    for (const auto& batch : batches)
    {
        if (m_suspended && !m_closed) { m_batches.push_back(batch); continue; }
        auto callback = std::move(batch->callback);
        const auto group = m_groups.find(batch->group);
        if (!m_closed && !batch->suppress_callback && callback && group != m_groups.end() &&
            group->second.generation == batch->generation) callback();
    }
}

void ResourceCenter::shutdown()
{
    if (m_closed) return;
    m_closed = true;
    m_catalog->shutdown();
    for (const auto& request : m_requests)
    {
        request->cancel();
        request->callback.reset();
    }
    for (const auto& batch : m_batches)
    {
        BatchLoadHandle(batch).cancel();
        batch->callback.reset();
    }
    for (const auto& [path, job] : m_jobs)
        if (job.submitted) m_backend->get(job.local_path);
    m_history.refresh();
    m_batch_history.refresh();
    m_jobs.clear();
    m_requests.clear();
    m_batches.clear();
    m_groups.clear();
    m_cache.clear();
    m_retained.clear();
    if (m_owned_downloads) m_owned_downloads->shutdown();
}
}
