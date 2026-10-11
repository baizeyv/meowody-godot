#pragma once
#include "time_clock.h"
#include "time_handles.h"
#include <godot_cpp/classes/packet_peer_udp.hpp>
namespace meowody::detail {
class NtpClient {
public:
    NtpClient(TimeClock& clock, NtpOptions options);
    ~NtpClient();
    void poll(SyncState& state);
    void close();
private:
    TimeClock& m_clock;
    NtpOptions m_options;
    godot::Ref<godot::PacketPeerUDP> m_udp;
    godot::PackedByteArray m_request;
    int m_resolver = -1;
    double m_started = 0, m_sent_ticks = 0, m_sent_unix = 0;
    bool m_started_request = false;
    void fail(SyncState& state, TimeSyncError error, const godot::String& message);
    void send(SyncState& state, const godot::String& address);
};
}
