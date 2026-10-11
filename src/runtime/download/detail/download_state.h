#pragma once
#include "../download_types.h"
#include "runtime/core/delegate.hpp"
#include <vector>

namespace meowody::detail
{
struct DownloadState
{
    DownloadStatus status = DownloadStatus::Queued;
    DownloadResult result;
    std::int64_t received = 0;
    std::int64_t total = -1;
    int retries = 0;
    bool suppress_callback = false;
    std::vector<Delegate<void(const DownloadResult&)>> callbacks;
};
}
