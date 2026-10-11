#pragma once
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <memory>
namespace meowody {
enum class NetStatus { Unknown, NoInterface, LocalOnly, Checking, Online, Unreachable };
struct NetSnapshot {
    NetStatus status = NetStatus::Unknown;
    bool has_interface = false;
    int consecutive_failures = 0;
    double last_check_seconds = 0, latency_ms = 0;
    godot::String probe_url;
};
enum class NetRequestStatus { Pending, Running, Succeeded, Failed, Cancelled };
enum class NetError { None, NotReady, InvalidArgument, Transport, Timeout, HTTPStatus, TooLarge, Cancelled, Closed };
struct NetResult {
    NetError error = NetError::NotReady;
    int status_code = 0;
    godot::PackedStringArray headers;
    godot::PackedByteArray body;
    godot::String message;
    bool ok() const { return error == NetError::None; }
    godot::String text() const { return body.get_string_from_utf8(); }
};
struct NetRequestOptions {
    godot::String url;
    godot::HTTPClient::Method method = godot::HTTPClient::METHOD_GET;
    godot::PackedStringArray headers;
    godot::PackedByteArray body;
    double timeout_seconds = 15;
    int64_t max_response_bytes = 8 * 1024 * 1024;
};
namespace detail {
struct NetState {
    NetRequestStatus status = NetRequestStatus::Pending;
    NetResult result;
    int64_t total = -1;
    bool suppress = false;
};
}
class NetCenter;
class NetHandle {
public:
    bool valid() const { return bool(m_state); }
    NetRequestStatus status() const { return m_state ? m_state->status : NetRequestStatus::Cancelled; }
    bool is_finished() const { auto s = status(); return s == NetRequestStatus::Succeeded || s == NetRequestStatus::Failed || s == NetRequestStatus::Cancelled; }
    NetResult result() const { return m_state ? m_state->result : NetResult{}; }
    double progress_percent() const {
        if (is_finished()) return 100;
        return m_state && m_state->total > 0 ? std::min(99.0, m_state->result.body.size() * 100.0 / m_state->total) : 0;
    }
    void cancel() {
        if (!m_state) return; m_state->suppress = true;
        if (!is_finished()) { m_state->status = NetRequestStatus::Cancelled; m_state->result.error = NetError::Cancelled; }
    }
private:
    friend class NetCenter;
    std::shared_ptr<detail::NetState> m_state;
};
}
