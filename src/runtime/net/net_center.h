#pragma once
#include "net_types.h"
#include "runtime/core/delegate.hpp"
#include <map>
#include <vector>
namespace meowody {
namespace detail { struct NetJob; }
class NetCenter {
public:
    explicit NetCenter(Delegate<bool()> interface_check = {});
    ~NetCenter();
    NetCenter(const NetCenter&) = delete;
    NetCenter& operator=(const NetCenter&) = delete;
    NetHandle request(NetRequestOptions options, Delegate<void(const NetResult&)> callback = {});
    NetHandle get(const godot::String& url, Delegate<void(const NetResult&)> callback = {});
    NetHandle post_json(const godot::String& url, const godot::String& json, Delegate<void(const NetResult&)> callback = {});
    bool configure_monitor(const godot::String& url, double interval_seconds = 5, int failure_threshold = 2);
    void check_now();
    NetSnapshot snapshot() const { return m_snapshot; }
    bool is_online() const { return m_snapshot.status == NetStatus::Online; }
    uint64_t subscribe(Delegate<void(const NetSnapshot&)> callback);
    bool unsubscribe(uint64_t id);
    size_t pending_count() const;
    bool set_max_concurrent(size_t count) { if (m_closed || count == 0) return false; m_max_concurrent = count; return true; }
    size_t max_concurrent() const { return m_max_concurrent; }
    void cancel_all();
    void update();
    void shutdown();
    bool is_closed() const { return m_closed; }
private:
    std::vector<std::shared_ptr<detail::NetJob>> m_jobs;
    std::map<uint64_t, std::shared_ptr<Delegate<void(const NetSnapshot&)>>> m_listeners;
    Delegate<bool()> m_interface_check;
    NetSnapshot m_snapshot;
    NetHandle m_probe;
    double m_interval = 5, m_next_probe = 0, m_probe_started = 0, m_next_interface = 0;
    int m_threshold = 2;
    uint64_t m_next_id = 1;
    size_t m_max_concurrent = 8;
    bool m_closed = false, m_updating = false, m_changed = false;
    void monitor(double now);
    void set_status(NetStatus status);
};
}
