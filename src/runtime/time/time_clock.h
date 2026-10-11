#pragma once
#include "time_types.h"
#include <memory>
namespace meowody::detail {
class TimeClock {
public:
    virtual ~TimeClock() = default;
    virtual double system_unix() const = 0;
    virtual double monotonic() const = 0;
    virtual DateTime calendar(double unix_seconds, TimeZone zone) const = 0;
};
DateTime calendar_from_unix(double unix_seconds, TimeZone zone);
std::unique_ptr<TimeClock> make_time_clock();
}
