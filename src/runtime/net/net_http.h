#pragma once
#include "net_types.h"
#include "runtime/core/delegate.hpp"
#include "runtime/download/http_url.h"
namespace meowody::detail {
struct NetJob {
    std::shared_ptr<NetState> state = std::make_shared<NetState>();
    NetRequestOptions options;
    Delegate<void(const NetResult&)> callback;
    godot::Ref<godot::HTTPClient> client;
    HttpUrl url;
    double started = 0;
    bool requested = false, received = false;
    void poll(double now);
    void fail(NetError error, const godot::String& message);
};
}
