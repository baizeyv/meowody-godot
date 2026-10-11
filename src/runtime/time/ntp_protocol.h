#pragma once
#include "time_types.h"
#include <godot_cpp/variant/packed_byte_array.hpp>
namespace meowody::detail {
void write_ntp_timestamp(godot::PackedByteArray& packet, int position, double unix_seconds);
double read_ntp_timestamp(const godot::PackedByteArray& packet, int position, double reference_unix);
TimeSyncResult decode_ntp(const godot::PackedByteArray& response, const godot::PackedByteArray& request,
    double sent_unix, double elapsed, double max_round_trip);
}
