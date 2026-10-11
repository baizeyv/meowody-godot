#include "time_center.h"
#include "detail/time_state.h"
#include <cmath>
#include <algorithm>
namespace meowody {
TimeCenter::TimeCenter() : TimeCenter(detail::make_time_clock()) {}
TimeCenter::TimeCenter(std::unique_ptr<detail::TimeClock> clock) : m_clock(clock ? std::move(clock) : detail::make_time_clock()) {}
TimeCenter::~TimeCenter() { shutdown(); }
double TimeCenter::system_unix_time() const { return m_clock->system_unix(); }
double TimeCenter::monotonic_seconds() const { return m_clock->monotonic(); }
double TimeCenter::network_sample_age() const { return m_sample_ticks < 0 ? -1 : std::max(0.0, monotonic_seconds() - m_sample_ticks); }
bool TimeCenter::network_synced() const { return !m_closed && m_sample_ticks >= 0; }
TimeReading TimeCenter::now(TimeSource source) const {
    if (source != TimeSource::System && network_synced()) return {true, true, m_sample_unix + network_sample_age()};
    if (source == TimeSource::Network) return {};
    double seconds = system_unix_time(); return {std::isfinite(seconds), false, seconds};
}
DateTime TimeCenter::to_datetime(double seconds, TimeZone zone) const { return m_clock->calendar(seconds, zone); }
DateTime TimeCenter::datetime(TimeZone zone, TimeSource source) const {
    auto reading = now(source); return reading.available ? to_datetime(reading.unix_seconds, zone) : DateTime{};
}
godot::String TimeCenter::format(TimeZone zone, TimeSource source) const { return datetime(zone, source).iso8601(); }
bool TimeCenter::is_syncing() const {
    if (!m_sync || *m_sync->cancelled) return false;
    auto s = m_sync->state->status;
    return s == TimeSyncStatus::Pending || s == TimeSyncStatus::Resolving || s == TimeSyncStatus::Waiting;
}
TimeSyncHandle TimeCenter::sync_network(const godot::String& server, Delegate<void(const TimeSyncResult&)> callback) {
    NtpOptions options; options.server = server; return sync_network(options, std::move(callback));
}
TimeSyncHandle TimeCenter::sync_network(const NtpOptions& options, Delegate<void(const TimeSyncResult&)> callback) {
    auto job = std::make_shared<detail::SyncJob>(); job->callback = std::move(callback); job->state->result.server = options.server;
    TimeSyncHandle handle; handle.m_state = job->state; handle.m_cancelled = job->cancelled;
    auto fail = [&](TimeSyncError error, const char* message) {
        job->state->status = TimeSyncStatus::Failed; job->state->result.error = error; job->state->result.message = message;
        if (!m_closed) m_delivery.push_back(job);
        return handle;
    };
    if (m_closed) return fail(TimeSyncError::Closed, "TimeCenter is closed");
    if (network_synced()) {
        job->state->status = TimeSyncStatus::Succeeded; job->state->result = m_success;
        job->state->result.unix_seconds = now(TimeSource::Network).unix_seconds;
        m_delivery.push_back(job); return handle;
    }
    if (options.server.is_empty() || options.server.length() > 253 || options.port < 1 || options.port > 65535 ||
        !std::isfinite(options.timeout_seconds) || options.timeout_seconds <= 0 || options.timeout_seconds > 60 ||
        !std::isfinite(options.max_round_trip_seconds) || options.max_round_trip_seconds <= 0)
        return fail(TimeSyncError::InvalidArgument, "Invalid NTP server, port or timing options");
    if (is_syncing()) return fail(TimeSyncError::Busy, "An NTP request is already active");
    if (m_denied_servers.contains(options.server)) return fail(TimeSyncError::ServerRejected, "Server previously denied requests; choose another server");
    if (m_next_sync >= 0 && monotonic_seconds() < m_next_sync) return fail(TimeSyncError::RateLimited, "Wait at least 64 seconds between NTP attempts");
    job->client = std::make_unique<detail::NtpClient>(*m_clock, options); m_sync = job;
    return handle;
}
void TimeCenter::cancel_sync() {
    if (!m_sync) return;
    TimeSyncHandle handle; handle.m_state = m_sync->state; handle.m_cancelled = m_sync->cancelled; handle.cancel();
    m_sync.reset();
}
void TimeCenter::update() {
    if (m_closed || m_updating || (!m_sync && m_delivery.empty() && m_daily.empty())) return;
    m_updating = true;
    struct Guard {
        bool& flag; std::vector<std::shared_ptr<detail::DailyEntry>>& entries;
        ~Guard() { entries.clear(); flag = false; }
    } guard{m_updating, m_daily_snapshot};
    m_daily_snapshot.reserve(m_daily.size());
    for (auto& [id, entry] : m_daily) m_daily_snapshot.push_back(entry);
    auto deliveries = std::move(m_delivery); m_delivery.clear();
    auto job = m_sync;
    if (job) {
        if (!*job->cancelled && job->state->status == TimeSyncStatus::Pending) m_next_sync = monotonic_seconds() + 64;
        if (!*job->cancelled) job->client->poll(*job->state);
        auto status = job->state->status;
        if (*job->cancelled || status == TimeSyncStatus::Succeeded || status == TimeSyncStatus::Failed) {
            if (status == TimeSyncStatus::Succeeded && !*job->cancelled) {
                m_sample_unix = job->state->result.unix_seconds; m_sample_ticks = monotonic_seconds();
                m_success = job->state->result;
            }
            if (job->state->result.error == TimeSyncError::ServerRejected) m_denied_servers.insert(job->state->result.server);
            m_sync.reset(); job->client.reset(); deliveries.push_back(job);
        }
    }
    for (auto& item : deliveries) {
        if (m_closed) break;
        if (!*item->cancelled && item->callback) item->callback(item->state->result);
    }
    if (!m_closed) dispatch_daily(m_daily_snapshot);
    for (auto it = m_daily.begin(); it != m_daily.end();) {
        if (it->second->state->cancelled) it = m_daily.erase(it); else ++it;
    }
}
void TimeCenter::shutdown() {
    if (m_closed) return;
    m_closed = true; cancel_sync(); m_sample_ticks = -1; clear_subscriptions();
    for (auto& job : m_delivery) *job->cancelled = true;
    m_delivery.clear();
}
}
