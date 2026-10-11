#include "time_center.h"
#include "detail/time_state.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace meowody {
void TimeCenter::baseline(detail::DailyEntry& entry, bool catch_up) {
    auto reading = now(entry.options.source); if (!reading.available) return;
    auto calendar = to_datetime(reading.unix_seconds, entry.options.zone); if (!calendar.valid) return;
    int target = entry.options.hour * 3600 + entry.options.minute * 60 + entry.options.second;
    int day = calendar.date_key();
    entry.last_day = std::max(entry.last_day, day - ((catch_up || calendar.seconds_of_day() < target) ? 1 : 0));
    entry.initialized = true;
}
DailySubscription TimeCenter::subscribe_daily(DailyOptions options, Delegate<void(const DailyEvent&)> callback) {
    DailySubscription handle;
    if (m_closed || !callback || options.hour < 0 || options.hour > 23 || options.minute < 0 || options.minute > 59 || options.second < 0 || options.second > 59) return handle;
    auto entry = std::make_shared<detail::DailyEntry>(); entry->options = options; entry->callback = std::move(callback);
    entry->state->id = m_next_id++; baseline(*entry, options.catch_up_today);
    handle.m_state = entry->state; m_daily.emplace(entry->state->id, entry); return handle;
}
DailySubscription TimeCenter::daily(int hour, int minute, Delegate<void()> callback, TimeZone zone) {
    if (!callback) return {};
    DailyOptions options; options.hour = hour; options.minute = minute; options.zone = zone;
    return subscribe_daily(options, [callback = std::move(callback)](const DailyEvent&) mutable { callback(); });
}
void TimeCenter::dispatch_daily(const std::vector<std::shared_ptr<detail::DailyEntry>>& entries) {
    // Daily scheduling has second resolution; share conversions within this pass.
    std::array<double, 2> calendar_seconds{std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()};
    std::array<DateTime, 2> calendars;
    for (auto& entry : entries) {
        if (m_closed) break;
        auto& state = *entry->state;
        if (state.cancelled || state.paused) continue;
        if (state.restart) {
            if (!now(entry->options.source).available) continue;
            baseline(*entry, false); state.restart = false;
        }
        if (!entry->initialized) baseline(*entry, entry->options.catch_up_today);
        auto reading = now(entry->options.source); if (!reading.available) continue;
        const size_t zone = entry->options.zone == TimeZone::Local ? 0 : 1;
        const double seconds = std::floor(reading.unix_seconds);
        if (calendar_seconds[zone] != seconds) {
            calendars[zone] = to_datetime(reading.unix_seconds, entry->options.zone);
            calendar_seconds[zone] = seconds;
        }
        const auto& calendar = calendars[zone]; if (!calendar.valid) continue;
        int target = entry->options.hour * 3600 + entry->options.minute * 60 + entry->options.second;
        if (calendar.date_key() <= entry->last_day || calendar.seconds_of_day() < target) continue;
        entry->last_day = calendar.date_key(); // Commit before calling user code.
        DailyEvent event{state.id, calendar, reading.unix_seconds, reading.from_network};
        entry->callback(event);
    }
}
bool TimeCenter::unsubscribe(uint64_t id) {
    auto it = m_daily.find(id); if (it == m_daily.end()) return false;
    it->second->state->cancelled = true; m_daily.erase(it); return true;
}
bool TimeCenter::pause(uint64_t id) {
    auto it = m_daily.find(id); if (it == m_daily.end() || it->second->state->cancelled) return false;
    it->second->state->paused = true; return true;
}
bool TimeCenter::resume(uint64_t id) {
    auto it = m_daily.find(id); if (it == m_daily.end() || it->second->state->cancelled) return false;
    if (it->second->state->paused) { it->second->state->paused = false; it->second->state->restart = true; }
    return true;
}
size_t TimeCenter::subscription_count() const {
    size_t count = 0; for (const auto& [id, entry] : m_daily) if (!entry->state->cancelled) ++count; return count;
}
void TimeCenter::clear_subscriptions() {
    for (auto& [id, entry] : m_daily) entry->state->cancelled = true;
    m_daily.clear();
}
}
