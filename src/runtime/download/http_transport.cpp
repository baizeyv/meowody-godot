#include "http_transport.h"
#include <godot_cpp/classes/tls_options.hpp>

namespace meowody::detail
{
godot::Error HttpTransport::start(const HttpUrl& url, const godot::PackedStringArray& headers)
{
    close();
    m_url = url;
    m_headers = headers;
    m_client.instantiate();
    m_client->set_blocking_mode(false);
    m_client->set_read_chunk_size(65536);
    const auto tls = url.scheme == "https" ? godot::TLSOptions::client() : godot::Ref<godot::TLSOptions>();
    return m_client->connect_to_host(url.host, url.port, tls);
}
HttpEvent HttpTransport::next()
{
    if (m_client.is_null()) return {HttpEvent::Failed, 0, -1, godot::ERR_UNCONFIGURED};
    const auto error = m_client->poll();
    if (error != godot::OK) return {HttpEvent::Failed, 0, -1, error};
    const auto status = m_client->get_status();
    if (!m_requested && status == godot::HTTPClient::STATUS_CONNECTED)
    {
        const auto request_error = m_client->request(godot::HTTPClient::METHOD_GET, m_url.target, m_headers);
        if (request_error != godot::OK) return {HttpEvent::Failed, 0, -1, request_error};
        m_requested = true;
        return {};
    }
    if (m_requested && !m_response && m_client->has_response())
    {
        m_response = true;
        HttpEvent event{HttpEvent::Headers, m_client->get_response_code(), m_client->get_response_body_length()};
        for (const auto& line : m_client->get_response_headers())
        {
            const int colon = line.find(":");
            if (colon < 1) continue;
            const auto name = line.substr(0, colon).strip_edges().to_lower();
            // Duplicate singleton headers are rejected by merging them into an invalid value.
            const auto value = line.substr(colon + 1).strip_edges();
            auto [entry, inserted] = event.headers.emplace(name, value);
            if (!inserted) entry->second += "," + value;
        }
        return event;
    }
    if (m_response && status == godot::HTTPClient::STATUS_BODY)
    {
        auto chunk = m_client->read_response_body_chunk();
        if (!chunk.is_empty())
        {
            HttpEvent event;
            event.kind = HttpEvent::Data;
            event.data = std::move(chunk);
            return event;
        }
        return {};
    }
    if (m_response && status == godot::HTTPClient::STATUS_CONNECTED) return {HttpEvent::Done};
    if (status == godot::HTTPClient::STATUS_CANT_RESOLVE || status == godot::HTTPClient::STATUS_CANT_CONNECT ||
        status == godot::HTTPClient::STATUS_CONNECTION_ERROR || status == godot::HTTPClient::STATUS_TLS_HANDSHAKE_ERROR ||
        status == godot::HTTPClient::STATUS_DISCONNECTED)
        return {HttpEvent::Failed, 0, -1, godot::ERR_CONNECTION_ERROR};
    return {};
}
void HttpTransport::close()
{
    if (m_client.is_valid()) m_client->close();
    m_client.unref();
    m_requested = false;
    m_response = false;
}
}
