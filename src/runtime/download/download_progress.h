#pragma once
#include "detail/download_state.h"
#include <algorithm>

namespace meowody
{
struct DownloadProgress
{
    bool found = false;
    DownloadStatus status = DownloadStatus::Failed;
    DownloadError error = DownloadError::InvalidHandle;
    std::int64_t downloaded_bytes = 0;
    std::int64_t total_bytes = -1;
    int retries = 0;
    DownloadResult last_result;
    bool finished() const { return found && is_download_finished(status); }
    bool succeeded() const { return found && status == DownloadStatus::Succeeded; }
    bool size_known() const { return total_bytes >= 0; }
    float progress_percent() const
    {
        if (!found) return 0.0f;
        if (finished()) return 100.0f;
        return total_bytes > 0 ? std::clamp(100.0f * downloaded_bytes / total_bytes, 0.0f, 100.0f) : 0.0f;
    }
    static DownloadProgress snapshot(const detail::DownloadState& state)
    {
        return {true, state.status, state.result.error, state.received, state.total, state.retries, state.result};
    }
};
}
