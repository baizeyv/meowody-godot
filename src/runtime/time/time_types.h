#pragma once
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
namespace meowody {
enum class TimeZone { Local, UTC };
enum class TimeSource { System, Network, BestAvailable };
struct DateTime {
    bool valid = false;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, weekday = 0;
    int utc_offset_seconds = 0;
    bool daylight_saving = false;
    int date_key() const { return year * 10000 + month * 100 + day; }
    int seconds_of_day() const { return hour * 3600 + minute * 60 + second; }
    godot::String iso8601() const;
};
struct TimeReading { bool available = false; bool from_network = false; double unix_seconds = 0; };
enum class TimeSyncStatus { Pending, Resolving, Waiting, Succeeded, Failed, Cancelled };
enum class TimeSyncError { None, NotReady, InvalidArgument, Busy, RateLimited, DNS, Transport, Timeout, InvalidResponse, ServerRejected, Cancelled, Closed };
struct TimeSyncResult {
    TimeSyncError error = TimeSyncError::None;
    godot::String message;
    godot::String server;
    double unix_seconds = 0; // Corrected UTC at receipt.
    double offset_seconds = 0; // Compared with the system clock at request transmission.
    double round_trip_seconds = 0;
    bool ok() const { return error == TimeSyncError::None; }
};
struct NtpOptions {
    godot::String server; // Explicit configuration; no hardcoded public service.
    int port = 123;
    double timeout_seconds = 5;
    double max_round_trip_seconds = 3;
};
struct DailyOptions {
    int hour = 0, minute = 0, second = 0;
    TimeZone zone = TimeZone::Local;
    TimeSource source = TimeSource::BestAvailable;
    bool catch_up_today = false;
};
struct DailyEvent {
    uint64_t subscription_id = 0;
    DateTime date_time;
    double unix_seconds = 0;
    bool from_network = false;
};
}
