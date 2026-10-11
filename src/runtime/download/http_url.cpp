#include "http_url.h"
#include <limits>

namespace meowody::detail
{
bool parse_unsigned(const godot::String& text, std::int64_t& value)
{
    if (text.is_empty()) return false;
    value = 0;
    for (int i = 0; i < text.length(); ++i)
    {
        const auto ch = text[i];
        if (ch < '0' || ch > '9' || value > (INT64_MAX - (ch - '0')) / 10) return false;
        value = value * 10 + ch - '0';
    }
    return true;
}
HttpUrl parse_http_url(const godot::String& input)
{
    HttpUrl result;
    for (int i = 0; i < input.length(); ++i)
        if (input[i] <= 32 || input[i] == 127 || input[i] == '\\') return {};
    const auto url = input.get_slice("#", 0);
    const int scheme_end = url.find("://");
    if (scheme_end < 0) return {};
    result.scheme = url.substr(0, scheme_end).to_lower();
    if (result.scheme != "http" && result.scheme != "https") return {};
    auto rest = url.substr(scheme_end + 3);
    int end = rest.length();
    for (const auto delimiter : {"/", "?"})
    {
        const int pos = rest.find(delimiter);
        if (pos >= 0 && pos < end) end = pos;
    }
    result.authority = rest.substr(0, end).to_lower();
    if (result.authority.is_empty() || result.authority.contains("@")) return {};
    result.target = rest.substr(end);
    if (!result.target.begins_with("/")) result.target = "/" + result.target;
    result.port = result.scheme == "https" ? 443 : 80;
    godot::String port;
    if (result.authority.begins_with("["))
    {
        const int close = result.authority.find("]");
        if (close < 2) return {};
        result.host = result.authority.substr(1, close - 1);
        if (close + 1 < result.authority.length())
        {
            if (result.authority[close + 1] != ':') return {};
            port = result.authority.substr(close + 2);
            if (port.is_empty()) return {};
        }
    }
    else
    {
        result.host = result.authority.get_slice(":", 0);
        if (result.authority.count(":") > 1) return {};
        if (result.authority.contains(":"))
        {
            port = result.authority.get_slice(":", 1);
            if (port.is_empty()) return {};
        }
    }
    if (result.host.is_empty()) return {};
    if (!port.is_empty())
    {
        std::int64_t number;
        if (!parse_unsigned(port, number) || number < 1 || number > 65535) return {};
        result.port = static_cast<int>(number);
    }
    result.url = result.scheme + godot::String("://") + result.authority + result.target;
    return result;
}
godot::String redirect_url(const HttpUrl& base, const godot::String& location)
{
    godot::String url;
    if (location.contains("://")) url = location;
    else if (location.begins_with("//")) url = base.scheme + godot::String(":") + location;
    else if (location.begins_with("/")) url = base.scheme + godot::String("://") + base.authority + location;
    else if (location.begins_with("?")) url = base.scheme + godot::String("://") + base.authority + base.target.get_slice("?", 0) + location;
    else
    {
        auto path = (base.target.get_slice("?", 0).get_base_dir() + godot::String("/") + location.get_slice("?", 0)).simplify_path();
        if (!path.begins_with("/")) path = "/" + path;
        url = base.scheme + godot::String("://") + base.authority + path;
        if (location.contains("?")) url += "?" + location.substr(location.find("?") + 1);
    }
    return parse_http_url(url).url;
}
godot::String url_extension(const godot::String& url)
{
    const auto ext = parse_http_url(url).target.get_slice("?", 0).get_extension().to_lower();
    if (ext.is_empty() || ext.length() > 10) return "bin";
    for (int i = 0; i < ext.length(); ++i)
        if (!((ext[i] >= 'a' && ext[i] <= 'z') || (ext[i] >= '0' && ext[i] <= '9'))) return "bin";
    return ext;
}
bool parse_content_range(const godot::String& text, ContentRange& range)
{
    if (!text.begins_with("bytes ")) return false;
    const auto value = text.substr(6);
    if (value.count("/") != 1 || value.get_slice("/", 0).count("-") != 1) return false;
    return parse_unsigned(value.get_slice("/", 0).get_slice("-", 0), range.first) &&
           parse_unsigned(value.get_slice("/", 0).get_slice("-", 1), range.last) &&
           parse_unsigned(value.get_slice("/", 1), range.total) &&
           range.first <= range.last && range.last < range.total;
}
}
