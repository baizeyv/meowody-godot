#include "download_center.h"
#include "detail/download_task.h"
#include <algorithm>
#include <godot_cpp/classes/time.hpp>

namespace meowody
{
DownloadCenter::DownloadCenter(const godot::String& cache_directory) : m_directory(cache_directory.simplify_path()) {}
DownloadCenter::~DownloadCenter() { shutdown(); }
DownloadHandle DownloadCenter::download(const godot::String& url, const DownloadOptions& options,
                                       Delegate<void(const DownloadResult&)> callback)
{
    auto state = std::make_shared<detail::DownloadState>();
    const auto parsed = detail::parse_http_url(url);
    auto fail = [&](DownloadError error, const char* message) {
        state->status = DownloadStatus::Failed;
        state->result = {{}, error, godot::OK, 0, message};
        if (callback && !m_closed) state->callbacks.push_back(std::move(callback));
        if (!m_closed) m_immediate.push_back(state);
        return DownloadHandle(state);
    };
    if (m_closed) return fail(DownloadError::Closed, "DownloadCenter is closed");
    if (!parsed.valid()) return fail(DownloadError::InvalidUrl, "Expected an HTTP(S) URL without userinfo");
    if (!detail::valid_download_options(options)) return fail(DownloadError::InvalidOptions, "Invalid download options");
    const auto identity = detail::download_identity(parsed, options);
    auto existing = m_tasks.find(identity);
    if (existing != m_tasks.end())
    {
        if (!is_download_finished(existing->second->state->status))
        {
            if (!detail::same_download_options(options, existing->second->options))
                return fail(DownloadError::Busy, "Same cache entry is being downloaded with different options");
            if (callback) existing->second->state->callbacks.push_back(std::move(callback));
            return DownloadHandle(existing->second->state);
        }
        // Keep deferred completion callbacks for the previous request.
        existing->second->close();
        m_immediate.push_back(existing->second->state);
        m_tasks.erase(existing);
    }
    auto task = std::make_unique<detail::DownloadTask>();
    task->state = state;
    task->options = options;
    task->original = parsed;
    task->store.identity = identity;
    task->store.path = m_directory.path_join(identity + godot::String(".") + detail::url_extension(parsed.url));
    m_history.track(identity, state);
    if (callback) state->callbacks.push_back(std::move(callback));
    m_tasks.emplace(identity, std::move(task));
    return DownloadHandle(state);
}
godot::String DownloadCenter::get_cached_path(const godot::String& url, const DownloadOptions& options) const
{
    const auto parsed = detail::parse_http_url(url);
    if (!parsed.valid() || !detail::valid_download_options(options)) return {};
    detail::DownloadStore store;
    store.identity = detail::download_identity(parsed, options);
    store.path = m_directory.path_join(store.identity + godot::String(".") + detail::url_extension(parsed.url));
    return store.cached(options) ? store.path : godot::String();
}
bool DownloadCenter::remove_cached(const godot::String& url, const DownloadOptions& options)
{
    const auto parsed = detail::parse_http_url(url);
    if (!parsed.valid() || !detail::valid_download_options(options)) return false;
    detail::DownloadStore store;
    store.identity = detail::download_identity(parsed, options);
    if (m_tasks.contains(store.identity)) return false;
    store.path = m_directory.path_join(store.identity + godot::String(".") + detail::url_extension(parsed.url));
    return store.remove();
}
void DownloadCenter::set_max_concurrent(std::size_t count) { m_max_concurrent = std::max<std::size_t>(1, count); }
void DownloadCenter::update()
{
    if (m_closed || m_updating || is_idle()) return;
    m_updating = true;
    struct Guard { bool& value; ~Guard() { value = false; } } guard{m_updating};
    std::vector<std::shared_ptr<detail::DownloadState>> ready;
    ready.swap(m_immediate);
    std::size_t active = 0;
    for (const auto& [key, task] : m_tasks) active += task->active();
    const auto now = godot::Time::get_singleton()->get_ticks_msec();
    for (auto it = m_tasks.begin(); it != m_tasks.end();)
    {
        auto& task = *it->second;
        const bool was_active = task.active();
        task.tick(now, active < m_max_concurrent);
        if (!was_active && task.active()) ++active;
        if (was_active && !task.active()) --active;
        if (is_download_finished(task.state->status))
        {
            task.close();
            ready.push_back(task.state);
            it = m_tasks.erase(it);
        }
        else ++it;
    }
    m_history.refresh();
    for (const auto& state : ready)
    {
        auto callbacks = std::move(state->callbacks);
        for (auto& callback : callbacks)
            if (!m_closed && !state->suppress_callback && callback) callback(state->result);
    }
}
void DownloadCenter::shutdown()
{
    if (m_closed) return;
    m_closed = true;
    for (const auto& [key, task] : m_tasks)
    {
        DownloadHandle(task->state).cancel();
        task->state->callbacks.clear();
        task->close();
    }
    for (const auto& state : m_immediate) { state->suppress_callback = true; state->callbacks.clear(); }
    m_history.refresh();
    m_tasks.clear();
    m_immediate.clear();
}
}
