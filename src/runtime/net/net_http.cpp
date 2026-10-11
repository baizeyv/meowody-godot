#include "net_http.h"
#include <godot_cpp/classes/tls_options.hpp>
namespace meowody::detail {
using namespace godot;
void NetJob::fail(NetError error, const String& message) {
    state->status = NetRequestStatus::Failed; state->result.error = error; state->result.message = message;
    if (client.is_valid()) client->close();
}
void NetJob::poll(double now) {
    if (state->status == NetRequestStatus::Pending) {
        started = now; client.instantiate(); client->set_read_chunk_size(65536);
        auto tls = url.scheme == "https" ? TLSOptions::client() : Ref<TLSOptions>();
        if (client->connect_to_host(url.host, url.port, tls) != OK) { fail(NetError::Transport, "HTTP connection failed"); return; }
        state->status = NetRequestStatus::Running;
    }
    if (now - started >= options.timeout_seconds) { fail(NetError::Timeout, "HTTP request timed out"); return; }
    if (client->poll() != OK) { fail(NetError::Transport, "HTTP poll failed"); return; }
    auto status = client->get_status();
    if (status == HTTPClient::STATUS_CANT_RESOLVE || status == HTTPClient::STATUS_CANT_CONNECT || status == HTTPClient::STATUS_CONNECTION_ERROR || status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
        fail(NetError::Transport, "DNS, connection or TLS failure"); return;
    }
    if (!requested && status == HTTPClient::STATUS_CONNECTED) {
        if (client->request_raw(options.method, url.target, options.headers, options.body) != OK) { fail(NetError::Transport, "HTTP request rejected"); return; }
        requested = true; return;
    }
    if (!received && requested && client->has_response()) {
        received = true; state->result.status_code = client->get_response_code(); state->result.headers = client->get_response_headers();
        state->total = client->get_response_body_length();
        if (state->total > options.max_response_bytes) { fail(NetError::TooLarge, "Response exceeds configured limit"); return; }
    }
    if (received && status == HTTPClient::STATUS_BODY) {
        auto chunk = client->read_response_body_chunk();
        if (state->result.body.size() + chunk.size() > options.max_response_bytes) { fail(NetError::TooLarge, "Response exceeds configured limit"); return; }
        state->result.body.append_array(chunk); status = client->get_status();
    }
    if (received && (status == HTTPClient::STATUS_CONNECTED || status == HTTPClient::STATUS_DISCONNECTED)) {
        if (options.method != HTTPClient::METHOD_HEAD && state->total >= 0 && state->result.body.size() != state->total) { fail(NetError::Transport, "Truncated HTTP response"); return; }
        if (state->result.status_code < 200 || state->result.status_code >= 300) { fail(NetError::HTTPStatus, "Non-success HTTP status (redirects are not followed)"); return; }
        state->status = NetRequestStatus::Succeeded; state->result.error = NetError::None; client->close();
    } else if (!received && status == HTTPClient::STATUS_DISCONNECTED) fail(NetError::Transport, "Disconnected before response");
}
}
