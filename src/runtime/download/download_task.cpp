#include "detail/download_task.h"
#include <algorithm>
#include <cmath>

namespace meowody::detail
{
namespace
{
godot::String header(const HttpEvent& event, const godot::String& name)
{
    const auto it = event.headers.find(name);
    return it == event.headers.end() ? godot::String() : it->second;
}
}
void DownloadTask::close()
{
    m_transport.close();
    store.close();
    m_active = false;
}
void DownloadTask::fail(DownloadError error, const godot::String& message, godot::Error engine_error)
{
    close();
    state->status = DownloadStatus::Failed;
    state->result.error = error;
    state->result.godot_error = engine_error;
    state->result.message = message;
}
void DownloadTask::retry(DownloadError error, const godot::String& message, std::uint64_t now, godot::Error engine_error)
{
    close();
    if (state->retries >= options.max_retries) { fail(error, message, engine_error); return; }
    ++state->retries;
    state->status = DownloadStatus::Retrying;
    m_retry_at = now + static_cast<std::uint64_t>(options.retry_delay_seconds * 1000 * (1 << std::min(state->retries - 1, 6)));
}
void DownloadTask::begin(std::uint64_t now)
{
    if (!m_checked_cache)
    {
        m_checked_cache = true;
        if (options.use_cache && store.cached(options))
        {
            state->status = DownloadStatus::Succeeded;
            state->received = state->total = godot::FileAccess::get_size(store.path);
            state->result = {store.path, DownloadError::None, godot::OK, 0, {}, true};
            return;
        }
    }
    const auto error = store.prepare(options.resume && !m_force_full, options.max_bytes, m_offset);
    if (error != godot::OK) { fail(DownloadError::FileIO, "Cannot prepare download directory", error); return; }
    state->received = m_offset;
    state->total = m_offset > 0 ? store.metadata.total : -1;
    m_current = original;
    m_headers = options.headers;
    m_redirects = 0;
    connect(now);
}
void DownloadTask::connect(std::uint64_t now)
{
    auto headers = m_headers;
    headers.push_back("Accept-Encoding: identity");
    if (m_offset > 0)
    {
        headers.push_back("Range: bytes=" + godot::String::num_int64(m_offset) + godot::String("-"));
        headers.push_back("If-Range: " + store.metadata.validator);
    }
    m_have_headers = false;
    m_body_expected = -1;
    m_body_received = 0;
    m_last_activity = now;
    state->status = DownloadStatus::Connecting;
    const auto error = m_transport.start(m_current, headers);
    m_active = true;
    if (error != godot::OK) retry(DownloadError::Network, "HTTP connection failed", now, error);
}
void DownloadTask::restart_full(std::uint64_t now)
{
    if (m_restarted_full) { fail(DownloadError::Protocol, "Invalid range response after full restart"); return; }
    m_restarted_full = true;
    m_force_full = true;
    close();
    begin(now);
}
void DownloadTask::headers(const HttpEvent& event, std::uint64_t now)
{
    const int code = event.response_code;
    state->result.http_status = code;
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308)
    {
        if (++m_redirects > options.max_redirects) { fail(DownloadError::Protocol, "Too many redirects"); return; }
        const auto location = header(event, "location");
        const auto target = parse_http_url(redirect_url(m_current, location));
        if (location.is_empty() || !target.valid() || (m_current.scheme == "https" && target.scheme != "https"))
        { fail(DownloadError::Protocol, "Invalid redirect or HTTPS downgrade"); return; }
        if (target.host != m_current.host || target.port != m_current.port || target.scheme != m_current.scheme)
            m_headers.clear(); // Never forward caller credentials across origins.
        m_current = target;
        connect(now);
        return;
    }
    if (code == 416 && m_offset > 0) { restart_full(now); return; }
    if (code == 408 || code == 429 || code >= 500)
    { retry(DownloadError::Http, "Transient HTTP failure", now); return; }
    if (code != 200 && code != 206) { fail(DownloadError::Http, "HTTP response is not a downloadable representation"); return; }
    const auto encoding = header(event, "content-encoding").to_lower();
    if (!encoding.is_empty() && encoding != "identity")
    { fail(DownloadError::Protocol, "Compressed transfer cannot be safely resumed"); return; }

    DownloadMetadata metadata;
    metadata.identity = store.identity;
    const auto etag = header(event, "etag");
    if (!etag.is_empty() && !etag.begins_with("W/") && etag.begins_with("\"") && etag.ends_with("\""))
    { metadata.validator = etag; metadata.validator_header = "etag"; }
    else if (!header(event, "last-modified").is_empty())
    { metadata.validator = header(event, "last-modified"); metadata.validator_header = "last-modified"; }
    m_body_expected = event.content_length;
    if (code == 206)
    {
        ContentRange range;
        if (!parse_content_range(header(event, "content-range"), range) || range.first != m_offset ||
            (event.content_length >= 0 && event.content_length != range.last - range.first + 1) ||
            (m_offset > 0 && (header(event, store.metadata.validator_header) != store.metadata.validator ||
                             (store.metadata.total >= 0 && store.metadata.total != range.total))))
        {
            if (m_offset > 0) restart_full(now);
            else fail(DownloadError::Protocol, "Invalid Content-Range or validator");
            return;
        }
        metadata.total = range.total;
        m_body_expected = range.last - range.first + 1;
    }
    else
    {
        // Range ignored or If-Range mismatch: replace, never append a 200 response.
        m_offset = 0;
        metadata.total = event.content_length;
    }
    if (metadata.total > options.max_bytes) { fail(DownloadError::TooLarge, "Response exceeds download size limit"); return; }
    const auto error = store.open_body(m_offset > 0, metadata);
    if (error != godot::OK) { fail(DownloadError::FileIO, "Cannot open partial file or save resume metadata", error); return; }
    state->received = m_offset;
    state->total = metadata.total;
    state->status = DownloadStatus::Downloading;
    m_have_headers = true;
}
void DownloadTask::finish()
{
    DownloadError error;
    const auto engine_error = store.commit(options, state->received, error);
    if (engine_error != godot::OK) { fail(error, "Cannot verify or commit downloaded file", engine_error); return; }
    close();
    state->total = state->received;
    state->status = DownloadStatus::Succeeded;
    state->result.path = store.path;
    state->result.error = DownloadError::None;
}
void DownloadTask::tick(std::uint64_t now, bool may_start)
{
    if (is_download_finished(state->status) || state->status == DownloadStatus::Paused) { close(); return; }
    if (state->status == DownloadStatus::Queued && m_active) close();
    if (!m_active)
    {
        if (!may_start || (state->status == DownloadStatus::Retrying && now < m_retry_at)) return;
        begin(now);
        return;
    }
    if (now - m_last_activity >= options.timeout_seconds * 1000)
    { retry(DownloadError::Timeout, "Download inactivity timeout", now); return; }
    // Bound network/file work per frame, irrespective of body size.
    bool wrote = false;
    for (int i = 0; i < 4 && m_active; ++i)
    {
        const auto event = m_transport.next();
        if (event.kind == HttpEvent::Idle) break;
        if (event.kind == HttpEvent::Failed)
        { retry(DownloadError::Network, "HTTP stream disconnected", now, event.error); return; }
        m_last_activity = now;
        if (event.kind == HttpEvent::Headers) { headers(event, now); return; }
        if (event.kind == HttpEvent::Data)
        {
            if (!m_have_headers || store.file.is_null()) { fail(DownloadError::Protocol, "Body before valid headers"); return; }
            if (event.data.size() > options.max_bytes - state->received)
            { fail(DownloadError::TooLarge, "Body exceeds download size limit"); return; }
            if (m_body_expected >= 0 && event.data.size() > m_body_expected - m_body_received)
            { fail(DownloadError::Protocol, "Body exceeds declared length"); return; }
            if (!store.file->store_buffer(event.data))
            { fail(DownloadError::FileIO, "Cannot write partial file", store.file->get_error()); return; }
            m_body_received += event.data.size();
            state->received += event.data.size();
            wrote = true;
        }
        if (event.kind == HttpEvent::Done)
        {
            if (!m_have_headers || (m_body_expected >= 0 && m_body_received != m_body_expected))
            { retry(DownloadError::Network, "Truncated response body", now); return; }
            if (state->total > state->received)
            {
                if (store.metadata.validator.is_empty() || m_body_received == 0)
                { fail(DownloadError::Protocol, "Incomplete range without a resume validator"); return; }
                close();
                begin(now);
                return;
            }
            finish();
            return;
        }
    }
    // close()/commit() flush on terminal branches; partial ticks flush once.
    if (wrote && store.file.is_valid()) store.file->flush();
}
}
