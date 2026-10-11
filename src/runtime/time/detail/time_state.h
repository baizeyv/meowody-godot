#pragma once
#include "runtime/time/time_handles.h"
#include "runtime/time/ntp_client.h"
#include "runtime/core/delegate.hpp"
namespace meowody::detail {
struct SyncJob {
    std::shared_ptr<SyncState> state = std::make_shared<SyncState>();
    std::shared_ptr<bool> cancelled = std::make_shared<bool>(false);
    Delegate<void(const TimeSyncResult&)> callback;
    std::unique_ptr<NtpClient> client;
};
struct DailyEntry {
    std::shared_ptr<DailyState> state = std::make_shared<DailyState>();
    DailyOptions options;
    Delegate<void(const DailyEvent&)> callback;
    int last_day = 0;
    bool initialized = false;
};
}
