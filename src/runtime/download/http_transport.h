#pragma once
#include <map>
#include <godot_cpp/classes/http_client.hpp>
#include "http_url.h"

namespace meowody::detail
{
using HttpHeaders = std::map<godot::String, godot::String>;
struct HttpEvent
{
    enum Kind { Idle, Headers, Data, Done, Failed } kind = Idle;
    int response_code = 0;
    std::int64_t content_length = -1;
    godot::Error error = godot::OK;
    HttpHeaders headers;
    godot::PackedByteArray data;
};
class HttpTransport
{
public:
    godot::Error start(const HttpUrl& url, const godot::PackedStringArray& headers);
    HttpEvent next();
    void close();
private:
    godot::Ref<godot::HTTPClient> m_client;
    HttpUrl m_url;
    godot::PackedStringArray m_headers;
    bool m_requested = false;
    bool m_response = false;
};
}
