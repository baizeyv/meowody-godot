#include "download_handle.h"
#include <algorithm>

namespace meowody
{
float DownloadHandle::progress() const
{
    if (finished()) return 1.0f;
    return total_bytes() > 0 ? std::clamp(static_cast<float>(downloaded_bytes()) / total_bytes(), 0.0f, 1.0f) : 0.0f;
}
DownloadResult DownloadHandle::result() const
{
    return m_state ? m_state->result : DownloadResult{{}, DownloadError::InvalidHandle, godot::OK, 0, "Invalid download handle"};
}
void DownloadHandle::pause() const
{
    if (m_state && !finished()) m_state->status = DownloadStatus::Paused;
}
void DownloadHandle::resume() const
{
    if (m_state && status() == DownloadStatus::Paused) m_state->status = DownloadStatus::Queued;
}
void DownloadHandle::cancel() const
{
    if (!m_state) return;
    m_state->suppress_callback = true;
    if (finished()) return;
    m_state->status = DownloadStatus::Cancelled;
    m_state->result = {{}, DownloadError::Cancelled, godot::OK, 0, "Download cancelled; partial file retained"};
}
}
