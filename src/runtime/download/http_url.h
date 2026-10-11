#pragma once
#include <cstdint>
#include <godot_cpp/variant/string.hpp>

namespace meowody::detail
{
struct HttpUrl
{
    godot::String url, scheme, host, authority, target;
    int port = 0;
    bool valid() const { return !url.is_empty(); }
};
HttpUrl parse_http_url(const godot::String& input);
godot::String redirect_url(const HttpUrl& base, const godot::String& location);
godot::String url_extension(const godot::String& url);
bool parse_unsigned(const godot::String& text, std::int64_t& value);
struct ContentRange { std::int64_t first = 0, last = 0, total = 0; };
bool parse_content_range(const godot::String& text, ContentRange& range);
}
