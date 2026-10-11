#pragma once
#include <memory>
#include "detail/download_state.h"

namespace meowody
{
class DownloadHandle
{
public:
    DownloadHandle() = default;
    bool valid() const { return static_cast<bool>(m_state); }
    DownloadStatus status() const { return m_state ? m_state->status : DownloadStatus::Failed; }
    bool finished() const { return is_download_finished(status()); }
    std::int64_t downloaded_bytes() const { return m_state ? m_state->received : 0; }
    std::int64_t total_bytes() const { return m_state ? m_state->total : -1; }
    int retries() const { return m_state ? m_state->retries : 0; }
    float progress() const;
    float progress_percent() const { return valid() ? progress() * 100.0f : 0.0f; }
    bool is_finished() const { return finished(); }
    bool succeeded() const { return status() == DownloadStatus::Succeeded; }
    bool failed() const { return status() == DownloadStatus::Failed; }
    bool cancelled() const { return status() == DownloadStatus::Cancelled; }
    bool paused() const { return status() == DownloadStatus::Paused; }
    bool size_known() const { return total_bytes() >= 0; }
    DownloadResult result() const;
    void pause() const;
    void resume() const;
    void cancel() const;
private:
    friend class DownloadCenter;
    explicit DownloadHandle(std::shared_ptr<detail::DownloadState> state) : m_state(std::move(state)) {}
    std::shared_ptr<detail::DownloadState> m_state;
};
}
