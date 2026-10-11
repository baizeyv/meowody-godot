#pragma once
#include "download_handle.h"
#include "download_progress.h"
#include "runtime/core/request_history.h"
#include <map>

namespace meowody
{
namespace detail { class DownloadTask; }
class DownloadCenter
{
public:
    explicit DownloadCenter(const godot::String& cache_directory = "user://meowody/downloads");
    ~DownloadCenter();
    DownloadCenter(const DownloadCenter&) = delete;
    DownloadCenter& operator=(const DownloadCenter&) = delete;
    DownloadHandle download(const godot::String& url, const DownloadOptions& options = {},
                            Delegate<void(const DownloadResult&)> callback = {});
    godot::String get_cached_path(const godot::String& url, const DownloadOptions& options = {}) const;
    bool remove_cached(const godot::String& url, const DownloadOptions& options = {});
    bool has_cached(const godot::String& url, const DownloadOptions& options = {}) const { return !get_cached_path(url, options).is_empty(); }
    DownloadHandle get_handle(const godot::String& url, const DownloadOptions& options = {}) const;
    DownloadProgress query(const godot::String& url, const DownloadOptions& options = {}) const;
    DownloadResult get_result(const godot::String& url, const DownloadOptions& options = {}) const;
    float progress_percent(const godot::String& url, const DownloadOptions& options = {}) const { return query(url, options).progress_percent(); }
    bool is_finished(const godot::String& url, const DownloadOptions& options = {}) const { return query(url, options).finished(); }
    bool pause(const godot::String& url, const DownloadOptions& options = {});
    bool resume(const godot::String& url, const DownloadOptions& options = {});
    bool cancel(const godot::String& url, const DownloadOptions& options = {});
    void cancel_all();
    std::size_t pending_count() const;
    bool is_idle() const { return m_tasks.empty() && m_immediate.empty(); }
    bool is_closed() const { return m_closed; }
    godot::String cache_directory() const { return m_directory; }
    std::size_t max_concurrent() const { return m_max_concurrent; }
    void set_max_concurrent(std::size_t count);
    void update();
    void shutdown();
private:
    godot::String m_directory;
    std::map<godot::String, std::unique_ptr<detail::DownloadTask>> m_tasks;
    std::vector<std::shared_ptr<detail::DownloadState>> m_immediate;
    std::size_t m_max_concurrent = 4;
    detail::RequestHistory<detail::DownloadState, DownloadProgress> m_history{DownloadProgress::snapshot};
    bool m_updating = false;
    bool m_closed = false;
};
}
