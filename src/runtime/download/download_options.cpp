#include "detail/download_task.h"
#include <cmath>

namespace meowody::detail
{
bool valid_download_options(const DownloadOptions& o)
{
    if (o.max_bytes <= 0 || o.max_retries < 0 || o.max_retries > 100 || o.max_redirects < 0 || o.max_redirects > 50 ||
        !std::isfinite(o.timeout_seconds) || o.timeout_seconds <= 0 || o.timeout_seconds > 86400 ||
        !std::isfinite(o.retry_delay_seconds) || o.retry_delay_seconds < 0 || o.retry_delay_seconds > 3600) return false;
    if (!o.expected_sha256.is_empty())
    {
        if (o.expected_sha256.length() != 64) return false;
        const auto hash = o.expected_sha256.to_lower();
        for (int i = 0; i < hash.length(); ++i)
            if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) return false;
    }
    for (const auto& line : o.headers)
    {
        if (line.contains("\r") || line.contains("\n") || line.find(":") <= 0) return false;
        const auto name = line.get_slice(":", 0).strip_edges().to_lower();
        if (name == "range" || name == "if-range" || name == "accept-encoding" || name == "host" ||
            name == "connection" || name == "content-length" || name == "transfer-encoding") return false;
    }
    return true;
}
bool same_download_options(const DownloadOptions& a, const DownloadOptions& b)
{
    return a.resume == b.resume && a.use_cache == b.use_cache && a.max_retries == b.max_retries &&
           a.max_redirects == b.max_redirects && a.timeout_seconds == b.timeout_seconds &&
           a.retry_delay_seconds == b.retry_delay_seconds && a.max_bytes == b.max_bytes &&
           a.expected_sha256.to_lower() == b.expected_sha256.to_lower() && a.headers == b.headers;
}
godot::String download_identity(const HttpUrl& url, const DownloadOptions& options)
{
    // Hash headers too, isolating authenticated representations without persisting credentials.
    return (url.url + godot::String("\n") + godot::String("\n").join(options.headers)).sha256_text();
}
}
