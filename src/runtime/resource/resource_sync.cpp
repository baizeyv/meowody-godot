#include "resource_center.h"
#include "runtime/download/http_url.h"
#include <algorithm>
namespace meowody {

LoadResult<> ResourceCenter::load_raw(const godot::String& input)
{
    if (m_closed) return {{}, LoadError::Closed, godot::OK, "ResourceCenter is closed"};
    const auto key = normalize(input);
    if (key.is_empty()) return {{}, LoadError::InvalidPath, godot::OK, "Invalid resource path or UID"};
    if (auto resource = cached(key); resource.is_valid()) return {resource};
    if (const auto job = m_jobs.find(key); job != m_jobs.end() && job->second.submitted)
    {
        const auto resource = m_backend->get(job->second.local_path);
        const auto result = resource.is_valid() ? LoadResult<>{resource} :
            LoadResult<>{{}, LoadError::LoadFailed, godot::OK, "ResourceLoader returned no resource"};
        finish_job(key, result);
        return result;
    }
    auto path = key;
    if (detail::parse_http_url(key).valid())
    {
        if (!detail::RemoteResourceLoader::supports(key))
            return {{}, LoadError::UnsupportedRemoteFormat, godot::OK, "Unsupported remote resource format"};
        path = m_downloads->get_cached_path(key);
        if (path.is_empty()) return {{}, LoadError::RemoteRequiresAsync, godot::OK, "Remote file is not cached; use load_async"};
        const auto prepared = m_remote.prepare(path);
        if (prepared.native_path.is_empty())
        {
            finish_job(key, prepared.result);
            if (prepared.result) remember(key, prepared.result.resource);
            return prepared.result;
        }
        path = prepared.native_path;
    }
    const auto resource = m_backend->load(path);
    const auto result = resource.is_valid() ? LoadResult<>{resource} :
        LoadResult<>{{}, LoadError::LoadFailed, godot::OK, "ResourceLoader returned no resource"};
    finish_job(key, result);
    if (result) remember(key, resource);
    return result;
}

}
