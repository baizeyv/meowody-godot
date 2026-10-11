#pragma once
#include "time_handles.h"
#include "time_clock.h"
#include "runtime/core/delegate.hpp"
#include <map>
#include <vector>
#include <set>
namespace meowody {
namespace detail { struct DailyEntry; struct SyncJob; }
class TimeCenter {
public:
    TimeCenter();
    explicit TimeCenter(std::unique_ptr<detail::TimeClock> clock);
    ~TimeCenter();
    TimeCenter(const TimeCenter&) = delete;
    TimeCenter& operator=(const TimeCenter&) = delete;
    double system_unix_time() const;
    double monotonic_seconds() const;
    TimeReading now(TimeSource source = TimeSource::BestAvailable) const;
    DateTime datetime(TimeZone zone = TimeZone::Local, TimeSource source = TimeSource::BestAvailable) const;
    DateTime to_datetime(double unix_seconds, TimeZone zone = TimeZone::UTC) const;
    godot::String format(TimeZone zone = TimeZone::Local, TimeSource source = TimeSource::BestAvailable) const;
    bool network_synced() const;
    double network_sample_age() const; // -1 if no successful sample.
    TimeSyncHandle sync_network(const NtpOptions& options, Delegate<void(const TimeSyncResult&)> callback = {});
    TimeSyncHandle sync_network(const godot::String& server, Delegate<void(const TimeSyncResult&)> callback = {});
    bool is_syncing() const;
    void cancel_sync();
    DailySubscription subscribe_daily(DailyOptions options, Delegate<void(const DailyEvent&)> callback);
    DailySubscription daily(int hour, int minute, Delegate<void()> callback, TimeZone zone = TimeZone::Local);
    bool unsubscribe(uint64_t id);
    bool pause(uint64_t id);
    bool resume(uint64_t id);
    size_t subscription_count() const;
    void clear_subscriptions();
    void update();
    void shutdown();
    bool is_closed() const { return m_closed; }
private:
    std::unique_ptr<detail::TimeClock> m_clock;
    std::shared_ptr<detail::SyncJob> m_sync;
    std::vector<std::shared_ptr<detail::SyncJob>> m_delivery;
    std::map<uint64_t, std::shared_ptr<detail::DailyEntry>> m_daily;
    std::vector<std::shared_ptr<detail::DailyEntry>> m_daily_snapshot;
    std::set<godot::String> m_denied_servers;
    uint64_t m_next_id = 1;
    double m_next_sync = -1, m_sample_unix = 0, m_sample_ticks = -1;
    TimeSyncResult m_success;
    bool m_closed = false, m_updating = false;
    void baseline(detail::DailyEntry& entry, bool catch_up);
    void dispatch_daily(const std::vector<std::shared_ptr<detail::DailyEntry>>& entries);
};
}
