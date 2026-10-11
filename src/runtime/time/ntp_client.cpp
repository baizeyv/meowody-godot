#include "ntp_client.h"
#include "ntp_protocol.h"
#include <godot_cpp/classes/ip.hpp>
#include <godot_cpp/classes/crypto.hpp>
#include <cmath>
namespace meowody::detail {
using namespace godot;
NtpClient::NtpClient(TimeClock& clock, NtpOptions options) : m_clock(clock), m_options(std::move(options)) {}
NtpClient::~NtpClient() { close(); }
void NtpClient::close() {
    if (m_resolver != IP::RESOLVER_INVALID_ID) { IP::get_singleton()->erase_resolve_item(m_resolver); m_resolver = IP::RESOLVER_INVALID_ID; }
    if (m_udp.is_valid()) { m_udp->close(); m_udp.unref(); }
}
void NtpClient::fail(SyncState& state, TimeSyncError error, const String& message) {
    state.status = TimeSyncStatus::Failed; state.result.error = error; state.result.message = message; close();
}
void NtpClient::send(SyncState& state, const String& address) {
    m_udp.instantiate();
    // Only a resolved numeric address reaches this call, avoiding synchronous DNS.
    if (m_udp->connect_to_host(address, m_options.port) != OK) { fail(state, TimeSyncError::Transport, "Cannot connect UDP socket"); return; }
    m_request.resize(48); m_request.fill(0); m_request.set(0, 0x23); // VN=4, mode=client.
    m_sent_unix = m_clock.system_unix(); m_sent_ticks = m_clock.monotonic();
    if (!std::isfinite(m_sent_unix) || m_sent_unix < 0 || m_sent_unix >= 253402300800.0) {
        fail(state, TimeSyncError::InvalidArgument, "System date is outside supported range"); return;
    }
    write_ntp_timestamp(m_request, 40, m_sent_unix);
    Ref<Crypto> crypto; crypto.instantiate(); auto random = crypto->generate_random_bytes(2);
    if (random.size() == 2) { m_request.set(46, random[0]); m_request.set(47, random[1]); }
    if (m_udp->put_packet(m_request) != OK) { fail(state, TimeSyncError::Transport, "Cannot send NTP request"); return; }
    state.status = TimeSyncStatus::Waiting;
}
void NtpClient::poll(SyncState& state) {
    if (!m_started_request) {
        m_started_request = true; m_started = m_clock.monotonic();
        state.result.server = m_options.server;
        if (m_options.server.is_valid_ip_address()) send(state, m_options.server);
        else {
            m_resolver = IP::get_singleton()->resolve_hostname_queue_item(m_options.server, IP::TYPE_ANY);
            if (m_resolver == IP::RESOLVER_INVALID_ID) { fail(state, TimeSyncError::DNS, "DNS resolver queue unavailable"); return; }
            state.status = TimeSyncStatus::Resolving;
        }
    }
    if (state.status == TimeSyncStatus::Resolving) {
        auto* ip = IP::get_singleton(); auto status = ip->get_resolve_item_status(m_resolver);
        if (status == IP::RESOLVER_STATUS_DONE) {
            auto address = ip->get_resolve_item_address(m_resolver);
            ip->erase_resolve_item(m_resolver); m_resolver = IP::RESOLVER_INVALID_ID;
            if (address.is_empty()) { fail(state, TimeSyncError::DNS, "DNS returned no address"); return; }
            send(state, address);
        } else if (status == IP::RESOLVER_STATUS_ERROR || status == IP::RESOLVER_STATUS_NONE) {
            fail(state, TimeSyncError::DNS, "NTP hostname could not be resolved"); return;
        }
    }
    if (state.status == TimeSyncStatus::Waiting) {
        // Bound per-frame work even if malformed packets arrive continuously.
        for (int count = 0; count < 8 && m_udp->get_available_packet_count() > 0; ++count) {
            auto packet = m_udp->get_packet();
            auto result = decode_ntp(packet, m_request, m_sent_unix, m_clock.monotonic() - m_sent_ticks, m_options.max_round_trip_seconds);
            if (result.ok()) { result.server = m_options.server; state.result = result; state.status = TimeSyncStatus::Succeeded; close(); return; }
            if (result.error == TimeSyncError::ServerRejected) { fail(state, result.error, result.message); return; }
            // Unrelated, malformed or delayed replies do not consume the pending request.
        }
    }
    if ((state.status == TimeSyncStatus::Resolving || state.status == TimeSyncStatus::Waiting) && m_clock.monotonic() - m_started >= m_options.timeout_seconds)
        fail(state, TimeSyncError::Timeout, "NTP request timed out (DNS/UDP or invalid replies)");
}
}
