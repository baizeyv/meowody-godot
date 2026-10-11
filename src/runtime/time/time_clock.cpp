#include "time_clock.h"
#include <godot_cpp/classes/time.hpp>
#include <cmath>
#include <ctime>
#include <cstdio>
namespace meowody {
godot::String DateTime::iso8601() const {
    if (!valid) return {};
    char text[64];
    std::snprintf(text, sizeof(text), "%04d-%02d-%02dT%02d:%02d:%02d", year, month, day, hour, minute, second);
    godot::String result(text);
    if (!utc_offset_seconds) return result + godot::String("Z");
    int offset = std::abs(utc_offset_seconds);
    std::snprintf(text, sizeof(text), "%c%02d:%02d", utc_offset_seconds < 0 ? '-' : '+', offset / 3600, offset / 60 % 60);
    return result + godot::String(text);
}
}
namespace meowody::detail {
DateTime calendar_from_unix(double seconds, TimeZone zone) {
    DateTime result;
    if (!std::isfinite(seconds) || seconds < 0 || seconds >= 253402300800.0) return result;
    std::time_t raw = static_cast<std::time_t>(std::floor(seconds)); std::tm civil{};
#ifdef _WIN32
    if (zone == TimeZone::Local) { _tzset(); if (localtime_s(&civil, &raw)) return result; }
    else if (gmtime_s(&civil, &raw)) return result;
#else
    if (zone == TimeZone::Local) { tzset(); if (!localtime_r(&raw, &civil)) return result; }
    else if (!gmtime_r(&raw, &civil)) return result;
#endif
    result.valid = true; result.year = civil.tm_year + 1900; result.month = civil.tm_mon + 1; result.day = civil.tm_mday;
    result.hour = civil.tm_hour; result.minute = civil.tm_min; result.second = civil.tm_sec;
    result.weekday = civil.tm_wday; result.daylight_saving = civil.tm_isdst > 0;
    if (zone == TimeZone::Local) {
        godot::Dictionary date;
        date["year"] = result.year; date["month"] = result.month; date["day"] = result.day;
        date["hour"] = result.hour; date["minute"] = result.minute; date["second"] = result.second;
        result.utc_offset_seconds = int(godot::Time::get_singleton()->get_unix_time_from_datetime_dict(date) - raw);
    }
    return result;
}
class GodotTimeClock final : public TimeClock {
public:
    double system_unix() const override { return godot::Time::get_singleton()->get_unix_time_from_system(); }
    double monotonic() const override { return godot::Time::get_singleton()->get_ticks_usec() / 1000000.0; }
    DateTime calendar(double time, TimeZone zone) const override { return calendar_from_unix(time, zone); }
};
std::unique_ptr<TimeClock> make_time_clock() { return std::make_unique<GodotTimeClock>(); }
}
