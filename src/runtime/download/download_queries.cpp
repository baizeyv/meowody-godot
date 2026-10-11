#include "download_center.h"
#include "detail/download_task.h"

namespace meowody
{
DownloadHandle DownloadCenter::get_handle(const godot::String& url, const DownloadOptions& options) const
{
    const auto parsed = detail::parse_http_url(url);
    if (!parsed.valid() || !detail::valid_download_options(options)) return {};
    const auto it = m_tasks.find(detail::download_identity(parsed, options));
    return it == m_tasks.end() ? DownloadHandle() : DownloadHandle(it->second->state);
}
DownloadProgress DownloadCenter::query(const godot::String& url, const DownloadOptions& options) const
{
    const auto parsed = detail::parse_http_url(url);
    if (!parsed.valid() || !detail::valid_download_options(options)) return {};
    return m_history.query(detail::download_identity(parsed, options));
}
bool DownloadCenter::pause(const godot::String& url, const DownloadOptions& options)
{
    const auto handle = get_handle(url, options);
    if (!handle.valid() || handle.finished()) return false;
    handle.pause();
    return true;
}
DownloadResult DownloadCenter::get_result(const godot::String& url, const DownloadOptions& options) const
{
    const auto progress = query(url, options);
    return progress.found ? progress.last_result : DownloadResult{{}, DownloadError::InvalidHandle, godot::OK, 0, "No recent download for this URL"};
}
bool DownloadCenter::resume(const godot::String& url, const DownloadOptions& options)
{
    const auto handle = get_handle(url, options);
    if (!handle.paused()) return false;
    handle.resume();
    return true;
}
bool DownloadCenter::cancel(const godot::String& url, const DownloadOptions& options)
{
    const auto handle = get_handle(url, options);
    if (!handle.valid()) return false;
    handle.cancel();
    return true;
}
void DownloadCenter::cancel_all()
{
    for (const auto& [key, task] : m_tasks) DownloadHandle(task->state).cancel();
    for (const auto& state : m_immediate) DownloadHandle(state).cancel();
}
std::size_t DownloadCenter::pending_count() const
{
    std::size_t count = 0;
    for (const auto& [key, task] : m_tasks) count += !is_download_finished(task->state->status);
    return count;
}
}
