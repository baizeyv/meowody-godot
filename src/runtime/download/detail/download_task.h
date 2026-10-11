#pragma once
#include "download_state.h"
#include "../download_store.h"
#include "../http_transport.h"
#include <memory>

namespace meowody::detail
{
class DownloadTask
{
public:
    std::shared_ptr<DownloadState> state = std::make_shared<DownloadState>();
    DownloadOptions options;
    DownloadStore store;
    HttpUrl original;
    bool active() const { return m_active; }
    void tick(std::uint64_t now, bool may_start);
    void close();
private:
    void begin(std::uint64_t now);
    void connect(std::uint64_t now);
    void headers(const HttpEvent& event, std::uint64_t now);
    void finish();
    void fail(DownloadError error, const godot::String& message, godot::Error engine_error = godot::OK);
    void retry(DownloadError error, const godot::String& message, std::uint64_t now, godot::Error engine_error = godot::OK);
    void restart_full(std::uint64_t now);
    HttpTransport m_transport;
    HttpUrl m_current;
    godot::PackedStringArray m_headers;
    bool m_active = false;
    bool m_checked_cache = false;
    bool m_force_full = false;
    bool m_restarted_full = false;
    bool m_have_headers = false;
    int m_redirects = 0;
    std::int64_t m_offset = 0;
    std::int64_t m_body_expected = -1;
    std::int64_t m_body_received = 0;
    std::uint64_t m_last_activity = 0;
    std::uint64_t m_retry_at = 0;
};
bool valid_download_options(const DownloadOptions& options);
bool same_download_options(const DownloadOptions& a, const DownloadOptions& b);
godot::String download_identity(const HttpUrl& url, const DownloadOptions& options);
}
