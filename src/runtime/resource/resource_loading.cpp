#include "resource_center.h"
#include "runtime/download/http_url.h"
#include <algorithm>
namespace meowody {

void ResourceCenter::complete(detail::LoadState& state, const LoadResult<>& result)
{
    if (is_load_finished(state.status)) return;
    state.result = result;
    if (result && state.accepts && !state.accepts(result.resource))
        state.result = {{}, LoadError::TypeMismatch, godot::OK, "Resource type does not match requested C++ type"};
    state.status = state.result ? LoadStatus::Succeeded : LoadStatus::Failed;
    state.progress = 1.0f;
}
void ResourceCenter::finish_job(const godot::String& key, const LoadResult<>& result)
{
    const auto job = m_jobs.find(key);
    if (job == m_jobs.end()) return;
    bool keep = false;
    for (const auto& state : job->second.requests)
    {
        if (is_load_finished(state->status)) continue;
        complete(*state, result);
        keep |= state->status == LoadStatus::Succeeded;
    }
    if (keep) remember(key, result.resource);
    m_jobs.erase(job);
}
void ResourceCenter::submit_local(const godot::String& key)
{
    auto& job = m_jobs.at(key);
    const auto error = m_backend->request(job.local_path);
    if (error != godot::OK)
    {
        finish_job(key, {{}, LoadError::RequestFailed, error, "ResourceLoader rejected request"});
        return;
    }
    job.submitted = true;
    for (const auto& request : job.requests)
        if (!is_load_finished(request->status)) request->status = LoadStatus::Loading;
}
void ResourceCenter::enqueue(const godot::String& input, const std::shared_ptr<detail::LoadState>& state,
                             const godot::String& pack_entry)
{
    if (m_closed)
    {
        complete(*state, {{}, LoadError::Closed, godot::OK, "ResourceCenter is closed"});
        state->callback.reset();
        return;
    }
    m_requests.push_back(state);
    state->path = pack_entry.is_empty() ? normalize(input) : pack_key(input, pack_entry);
    m_history.track(state->path, state);
    if (state->path.is_empty())
    {
        complete(*state, {{}, LoadError::InvalidPath, godot::OK, "Invalid resource URL, path, UID or pack entry"});
        return;
    }
    if (auto resource = cached(state->path); resource.is_valid()) { complete(*state, {resource}); return; }
    const auto remote = detail::parse_http_url(input);
    if (remote.valid() && pack_entry.is_empty() && !detail::RemoteResourceLoader::supports(remote.url))
    {
        complete(*state, {{}, LoadError::UnsupportedRemoteFormat, godot::OK, "Use a supported extension, or load_pack_async for PCK/ZIP"});
        return;
    }
    auto [entry, inserted] = m_jobs.try_emplace(state->path);
    auto& job = entry->second;
    job.requests.push_back(state);
    state->status = remote.valid() && !job.submitted ? LoadStatus::Downloading : LoadStatus::Loading;
    if (!inserted) return;
    if (remote.valid())
    {
        job.source_url = remote.url;
        job.pack_entry = pack_entry.is_empty() ? godot::String() : normalize(pack_entry);
        job.download = m_downloads->download(remote.url);
    }
    else
    {
        job.local_path = state->path;
        submit_local(state->path);
    }
}
void ResourceCenter::advance_remote(const godot::String& key)
{
    auto& job = m_jobs.at(key);
    for (const auto& state : job.requests)
        if (!is_load_finished(state->status)) state->progress = job.download.progress() * 0.85f;
    if (!job.download.finished()) return;
    const auto downloaded = job.download.result();
    if (!downloaded)
    {
        finish_job(key, {{}, LoadError::DownloadFailed, downloaded.godot_error,
                        downloaded.message + godot::String(" (HTTP ") + godot::String::num_int64(downloaded.http_status) + godot::String(")")});
        return;
    }
    const bool wanted = std::any_of(job.requests.begin(), job.requests.end(), [](const auto& state) {
        return !is_load_finished(state->status);
    });
    if (!wanted) { m_jobs.erase(key); return; }
    const auto prepared = m_remote.prepare(downloaded.path, job.pack_entry);
    if (prepared.native_path.is_empty()) { finish_job(key, prepared.result); return; }
    job.local_path = prepared.native_path;
    submit_local(key);
}
void ResourceCenter::poll_jobs()
{
    m_poll_keys.clear();
    m_poll_keys.reserve(m_jobs.size());
    for (const auto& [key, job] : m_jobs) m_poll_keys.push_back(key);
    for (const auto& key : m_poll_keys)
    {
        const auto found = m_jobs.find(key);
        if (found == m_jobs.end()) continue;
        const auto& job = found->second;
        if (!job.submitted) { advance_remote(key); continue; }
        float progress = 0.0f;
        const auto status = m_backend->poll(job.local_path, progress);
        const float offset = job.source_url.is_empty() ? 0.0f : 0.85f;
        for (const auto& state : job.requests)
            if (!is_load_finished(state->status))
                state->progress = std::max(state->progress, offset + (1 - offset) * std::clamp(progress, 0.0f, 1.0f));
        if (status != godot::ResourceLoader::THREAD_LOAD_IN_PROGRESS)
        {
            const auto resource = m_backend->get(job.local_path);
            finish_job(key, status == godot::ResourceLoader::THREAD_LOAD_LOADED && resource.is_valid() ? LoadResult<>{resource} :
                       LoadResult<>{{}, LoadError::LoadFailed, godot::OK, "Threaded resource load failed"});
        }
    }
    m_poll_keys.clear();
}

}
