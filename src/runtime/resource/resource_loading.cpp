#include "resource_center.h"
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
void ResourceCenter::enqueue(const godot::String& input, const std::shared_ptr<detail::LoadState>& state)
{
    if (m_closed)
    {
        complete(*state, {{}, LoadError::Closed, godot::OK, "ResourceCenter is closed"});
        state->callback.reset();
        return;
    }
    m_requests.push_back(state);
    state->path = normalize(input);
    m_history.track(state->path, state);
    if (state->path.is_empty())
    {
        complete(*state, {{}, LoadError::InvalidPath, godot::OK, "Expected a configured resource address"});
        return;
    }
    if (auto resource = cached(state->path); resource.is_valid()) { complete(*state, {resource}); m_retained[state->path].push_back(state); return; }
    auto [entry, inserted] = m_jobs.try_emplace(state->path);
    auto& job = entry->second;
    job.requests.push_back(state);
    m_retained[state->path].push_back(state);
    state->status = LoadStatus::Loading;
    if (!inserted) return;
    job.local_path = m_catalog->path(state->path);
    advance_remote(state->path);
}
void ResourceCenter::advance_remote(const godot::String& key)
{
    const auto& requests = m_jobs.at(key).requests;
    if (std::none_of(requests.begin(), requests.end(), [](const auto& state) { return !is_load_finished(state->status); })) {
        m_jobs.erase(key);
        return;
    }
    auto error = m_catalog->prepare(key, true);
    if (error == LoadError::NotInitialized && m_catalog->error() == LoadError::None) return;
    if (error == LoadError::RemoteRequiresAsync) {
        for (const auto& state : m_jobs.at(key).requests) if (!is_load_finished(state->status)) {
            state->status = LoadStatus::Downloading;
            state->progress = m_catalog->download_progress(key) * 0.85f;
        }
        return;
    }
    if (error != LoadError::None) {
        finish_job(key, {{}, error, godot::OK, m_catalog->ready() ? godot::String("Cannot prepare address: ") + key : m_catalog->message()});
        return;
    }
    m_jobs.at(key).local_path = m_catalog->path(key);
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
        const float offset = 0.0f;
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
