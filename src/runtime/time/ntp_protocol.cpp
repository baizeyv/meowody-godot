#include "ntp_protocol.h"
#include <cmath>
#include <algorithm>
namespace meowody::detail {
constexpr double era_seconds = 4294967296.0;
constexpr double epoch_offset = 2208988800.0;
static uint32_t read_word(const godot::PackedByteArray& p, int at) {
    return uint32_t(p[at]) << 24 | uint32_t(p[at + 1]) << 16 | uint32_t(p[at + 2]) << 8 | p[at + 3];
}
static void write_word(godot::PackedByteArray& p, int at, uint32_t v) {
    for (int i = 0; i < 4; ++i) p.set(at + i, uint8_t(v >> (24 - 8 * i)));
}
void write_ntp_timestamp(godot::PackedByteArray& packet, int at, double unix_seconds) {
    double ntp = unix_seconds + epoch_offset;
    write_word(packet, at, uint32_t(uint64_t(std::floor(ntp)) & 0xffffffff));
    write_word(packet, at + 4, uint32_t((ntp - std::floor(ntp)) * era_seconds));
}
double read_ntp_timestamp(const godot::PackedByteArray& packet, int at, double reference) {
    double low = read_word(packet, at);
    double era = std::round((reference + epoch_offset - low) / era_seconds);
    return era * era_seconds + low + read_word(packet, at + 4) / era_seconds - epoch_offset;
}
TimeSyncResult decode_ntp(const godot::PackedByteArray& response, const godot::PackedByteArray& request,
        double sent, double elapsed, double max_round_trip) {
    TimeSyncResult result;
    auto fail = [&](TimeSyncError code, const char* reason) { result.error = code; result.message = reason; return result; };
    if (response.size() < 48 || request.size() < 48) return fail(TimeSyncError::InvalidResponse, "Short NTP packet");
    for (int i = 0; i < 8; ++i) if (response[24 + i] != request[40 + i]) return fail(TimeSyncError::InvalidResponse, "NTP originate timestamp mismatch");
    int version = (response[0] >> 3) & 7;
    if ((response[0] & 7) != 4 || (version != 3 && version != 4)) return fail(TimeSyncError::InvalidResponse, "Expected NTP v3/v4 server reply");
    if (response[1] == 0) return fail(TimeSyncError::ServerRejected, "Server sent Kiss-o'-Death; stop querying this server");
    if ((response[0] >> 6) == 3 || response[1] > 15) return fail(TimeSyncError::ServerRejected, "NTP server is not synchronized");
    if (!(read_word(response, 32) || read_word(response, 36)) || !(read_word(response, 40) || read_word(response, 44)))
        return fail(TimeSyncError::InvalidResponse, "Missing server timestamps");
    double receive = read_ntp_timestamp(response, 32, sent);
    double transmit = read_ntp_timestamp(response, 40, sent);
    double delay = elapsed - (transmit - receive);
    if (!std::isfinite(elapsed) || elapsed < 0 || transmit < receive || delay < -0.001 || delay > max_round_trip)
        return fail(TimeSyncError::InvalidResponse, "Invalid or excessive NTP round-trip delay");
    result.round_trip_seconds = std::max(0.0, delay);
    result.offset_seconds = ((receive - sent) + (transmit - (sent + elapsed))) / 2.0;
    result.unix_seconds = sent + elapsed + result.offset_seconds;
    if (!std::isfinite(result.unix_seconds) || result.unix_seconds < 0 || result.unix_seconds >= 253402300800.0)
        return fail(TimeSyncError::InvalidResponse, "NTP timestamp out of supported range");
    return result;
}
}
