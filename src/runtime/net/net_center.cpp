#include "net_center.h"
#include "net_http.h"
#include <godot_cpp/classes/ip.hpp>
#include <godot_cpp/classes/time.hpp>
#include <algorithm>
#include <optional>
#include <cmath>
namespace meowody {
using namespace godot;
NetCenter::NetCenter(Delegate<bool()> interface_check) : m_interface_check(std::move(interface_check)) {
    if (!m_interface_check) m_interface_check = [] {
        auto addresses = IP::get_singleton()->get_local_addresses();
        for (int i = 0; i < addresses.size(); ++i) {
            auto a = addresses[i];
            if (a != "::1" && a != "::" && a != "0.0.0.0" && !a.begins_with("127.")) return true;
        }
        return false;
    };
}
NetCenter::~NetCenter() { shutdown(); }
NetHandle NetCenter::request(NetRequestOptions options, Delegate<void(const NetResult&)> callback) {
    auto job = std::make_shared<detail::NetJob>(); job->options = std::move(options); job->callback = std::move(callback);
    job->url = detail::parse_http_url(job->options.url);
    NetHandle handle; handle.m_state = job->state;
    if (m_closed) { job->fail(NetError::Closed, "NetCenter is closed"); return handle; }
    if (!job->url.valid() || !std::isfinite(job->options.timeout_seconds) || job->options.timeout_seconds <= 0 || job->options.max_response_bytes < 0 ||
        job->options.method < HTTPClient::METHOD_GET || job->options.method >= HTTPClient::METHOD_MAX)
        job->fail(NetError::InvalidArgument, "Invalid HTTP request options");
    m_jobs.push_back(job); return handle;
}
NetHandle NetCenter::get(const String& url, Delegate<void(const NetResult&)> callback) { NetRequestOptions o; o.url = url; return request(std::move(o), std::move(callback)); }
NetHandle NetCenter::post_json(const String& url, const String& json, Delegate<void(const NetResult&)> callback) {
    NetRequestOptions o; o.url = url; o.method = HTTPClient::METHOD_POST; o.headers.append("Content-Type: application/json"); o.body = json.to_utf8_buffer(); return request(std::move(o), std::move(callback));
}
size_t NetCenter::pending_count() const { size_t n = 0; for (auto& j : m_jobs) if (j->state->status == NetRequestStatus::Pending || j->state->status == NetRequestStatus::Running) ++n; return n; }
void NetCenter::cancel_all() { for (auto& j : m_jobs) { NetHandle h; h.m_state = j->state; h.cancel(); } }
bool NetCenter::configure_monitor(const String& url, double interval, int failures) {
    if (m_closed || (!url.is_empty() && !detail::parse_http_url(url).valid()) || !std::isfinite(interval) || interval < 1 || failures < 1) return false;
    m_probe.cancel(); m_probe = {}; m_snapshot.probe_url = url; m_snapshot.consecutive_failures = 0;
    m_interval = interval; m_threshold = failures; m_next_probe = 0; m_next_interface = 0; set_status(NetStatus::Unknown); return true;
}
void NetCenter::check_now() { m_next_probe = 0; m_next_interface = 0; }
void NetCenter::set_status(NetStatus status) { if (status != m_snapshot.status) { m_snapshot.status = status; m_changed = true; } }
uint64_t NetCenter::subscribe(Delegate<void(const NetSnapshot&)> callback) {
    if (m_closed || !callback) return 0;
    auto id = m_next_id++; m_listeners.emplace(id, std::make_shared<Delegate<void(const NetSnapshot&)>>(std::move(callback))); return id;
}
bool NetCenter::unsubscribe(uint64_t id) { return m_listeners.erase(id) > 0; }
void NetCenter::monitor(double now) {
    if (now >= m_next_interface) {
        bool previously_up = m_snapshot.has_interface;
        m_snapshot.has_interface = m_interface_check(); m_next_interface = now + 1;
        if (!previously_up && m_snapshot.has_interface) m_next_probe = 0;
    }
    if (!m_snapshot.has_interface) { m_probe.cancel(); m_probe = {}; set_status(NetStatus::NoInterface); return; }
    if (m_snapshot.probe_url.is_empty()) { set_status(NetStatus::LocalOnly); return; }
    if (m_probe.valid()) {
        if (!m_probe.is_finished()) return;
        m_snapshot.last_check_seconds = now; m_snapshot.latency_ms = (now - m_probe_started) * 1000;
        if (m_probe.result().ok()) { m_snapshot.consecutive_failures = 0; set_status(NetStatus::Online); }
        else if (++m_snapshot.consecutive_failures >= m_threshold) set_status(NetStatus::Unreachable);
        m_probe = {}; m_next_probe = now + m_interval;
    }
    if (now >= m_next_probe) {
        if (m_snapshot.status != NetStatus::Online && m_snapshot.status != NetStatus::Unreachable) set_status(NetStatus::Checking);
        NetRequestOptions o; o.url = m_snapshot.probe_url; o.timeout_seconds = 5; o.max_response_bytes = 65536;
        m_probe = request(std::move(o)); m_probe_started = now;
    }
}
void NetCenter::update() {
    if (m_closed || m_updating) return;
    m_updating = true; struct Guard { bool& f; ~Guard() { f = false; } } guard{m_updating};
    double now = Time::get_singleton()->get_ticks_usec() / 1000000.0;
    std::vector<std::shared_ptr<detail::NetJob>> done;
    size_t running = 0;
    for (auto& j : m_jobs) if (j->state->status == NetRequestStatus::Running) ++running;
    std::erase_if(m_jobs, [&](const auto& j) {
        if (j->state->status == NetRequestStatus::Pending && running >= m_max_concurrent) return false;
        bool was_running = j->state->status == NetRequestStatus::Running;
        if (j->state->status == NetRequestStatus::Pending || j->state->status == NetRequestStatus::Running) j->poll(now);
        if (was_running && j->state->status != NetRequestStatus::Running) --running;
        if (!was_running && j->state->status == NetRequestStatus::Running) ++running;
        if (j->state->status == NetRequestStatus::Pending || j->state->status == NetRequestStatus::Running) return false;
        if (j->client.is_valid()) j->client->close(); done.push_back(j); return true;
    });
    monitor(now);
    std::optional<decltype(m_listeners)> listeners;
    if (m_changed) listeners.emplace(m_listeners);
    bool changed = m_changed; m_changed = false; auto snapshot = m_snapshot;
    for (auto& j : done) { if (m_closed) break; if (!j->state->suppress && j->callback) j->callback(j->state->result); }
    if (changed) for (auto& [id, cb] : *listeners) { if (m_closed) break; if (m_listeners.contains(id)) (*cb)(snapshot); }
}
void NetCenter::shutdown() { if (m_closed) return; m_closed = true; cancel_all(); m_probe.cancel(); m_jobs.clear(); m_listeners.clear(); }
}
