#include "scene_center.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/core/object.hpp>
#include <cmath>
#include <algorithm>
namespace meowody {
using namespace godot;
static SceneTree* tree() { return Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop()); }
SceneCenter::SceneCenter(ResourceCenter& resources) : m_resources(resources) {}
SceneCenter::~SceneCenter() { shutdown(); }
Node* SceneCenter::current_scene() const { auto* t = tree(); return t ? t->get_current_scene() : nullptr; }
SceneHandle SceneCenter::current_transition() const { return m_job ? m_job->handle : SceneHandle{}; }
bool SceneCenter::overlay() {
    if (ObjectDB::get_instance(m_layer) && ObjectDB::get_instance(m_rect)) return true;
    auto* t = tree(); if (!t || !t->get_root()) return false;
    auto* layer = memnew(CanvasLayer); layer->set_name("MeowodySceneTransition"); layer->set_layer(1024); layer->set_process_mode(Node::PROCESS_MODE_ALWAYS);
    t->get_root()->add_child(layer); auto* rect = memnew(ColorRect); layer->add_child(rect);
    rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT); rect->set_mouse_filter(Control::MOUSE_FILTER_STOP);
    m_layer = layer->get_instance_id(); m_rect = rect->get_instance_id(); opacity(0); return true;
}
void SceneCenter::opacity(float alpha) {
    auto* rect = Object::cast_to<ColorRect>(ObjectDB::get_instance(m_rect)); if (!rect) return;
    rect->set_color(Color(0, 0, 0, alpha)); rect->set_visible(alpha > 0);
}
SceneHandle SceneCenter::begin(LoadHandle<PackedScene> load, SceneOptions options, Delegate<void(const SceneResult&)> callback) {
    auto job = std::make_shared<Job>(); job->handle.m_state = std::make_shared<detail::SceneState>();
    job->load = std::move(load); job->options = options; job->callback = std::move(callback);
    if (m_closed || is_switching() || !std::isfinite(options.fade_in_seconds) || !std::isfinite(options.fade_out_seconds) || options.fade_in_seconds < 0 || options.fade_out_seconds < 0) {
        job->handle.m_state->status = SceneStatus::Failed; job->handle.m_state->result.message = "SceneCenter closed, busy, or invalid fade durations";
        job->load.cancel(); if (!m_closed) m_delivery.push_back(job); return job->handle;
    }
    m_job = job; return job->handle;
}
SceneHandle SceneCenter::switch_to(const String& path, SceneOptions options, Delegate<void(const SceneResult&)> callback) {
    return begin(m_closed || is_switching() ? LoadHandle<PackedScene>{} : m_resources.load_async<PackedScene>(path), options, std::move(callback));
}
SceneHandle SceneCenter::switch_pack(const String& url, const String& entry, SceneOptions options, Delegate<void(const SceneResult&)> callback) {
    return begin(m_closed || is_switching() ? LoadHandle<PackedScene>{} : m_resources.load_pack_async<PackedScene>(url, entry), options, std::move(callback));
}
SceneHandle SceneCenter::reload(SceneOptions options, Delegate<void(const SceneResult&)> callback) {
    auto* current = current_scene(); return switch_to(current ? current->get_scene_file_path() : String(), options, std::move(callback));
}
void SceneCenter::finish(Job& job, SceneStatus status, const String& message) {
    auto& state = *job.handle.m_state; state.status = status; state.result.message = message;
    state.result.succeeded = status == SceneStatus::Succeeded; state.progress = 100; opacity(0);
}
void SceneCenter::update(double delta) {
    if (m_closed || m_updating) return;
    m_updating = true; struct Guard { bool& v; ~Guard() { v = false; } } guard{m_updating};
    if (!std::isfinite(delta) || delta < 0) delta = 0;
    auto delivery = std::move(m_delivery); m_delivery.clear(); auto job = m_job;
    if (job) {
        auto& s = *job->handle.m_state;
        if (s.cancel && !s.committed) { job->load.cancel(); finish(*job, SceneStatus::Cancelled, "Cancelled before scene commit"); }
        else if (s.status == SceneStatus::Loading) {
            s.progress = job->load.progress_percent() * 0.8;
            if (job->load.is_finished()) {
                auto result = job->load.result();
                if (!result) finish(*job, SceneStatus::Failed, result.message);
                else if (!overlay()) finish(*job, SceneStatus::Failed, "SceneTree unavailable");
                else { s.status = SceneStatus::FadingOut; job->elapsed = 0; }
            }
        } else if (s.status == SceneStatus::FadingOut) {
            job->elapsed += delta; double fraction = job->options.fade_out_seconds > 0 ? std::min(1.0, job->elapsed / job->options.fade_out_seconds) : 1;
            opacity(fraction); s.progress = 80 + fraction * 10;
            if (fraction >= 1) { s.status = SceneStatus::Switching; job->elapsed = 0; }
        } else if (s.status == SceneStatus::Switching) {
            if (!s.committed) {
                auto* t = tree(); auto result = job->load.result();
                auto* node = result ? result.resource->instantiate() : nullptr;
                if (!t || !node) { if (node) memdelete(node); finish(*job, SceneStatus::Failed, "Cannot instantiate target scene"); }
                else {
                    job->next_scene = node->get_instance_id();
                    auto error = t->change_scene_to_node(node);
                    if (error != OK) { memdelete(node); finish(*job, SceneStatus::Failed, "SceneTree rejected target scene"); }
                    else s.committed = true;
                }
            } else {
                auto* current = current_scene();
                if (current && current->get_instance_id() == job->next_scene) {
                    s.result.scene_id = job->next_scene; s.status = SceneStatus::FadingIn; job->elapsed = 0;
                } else if (current || !ObjectDB::get_instance(job->next_scene)) finish(*job, SceneStatus::Failed, "Target scene was removed or replaced externally");
            }
        } else if (s.status == SceneStatus::FadingIn) {
            job->elapsed += delta; double fraction = job->options.fade_in_seconds > 0 ? std::min(1.0, job->elapsed / job->options.fade_in_seconds) : 1;
            opacity(1 - fraction); s.progress = 90 + fraction * 10;
            if (fraction >= 1) finish(*job, SceneStatus::Succeeded);
        }
        if (job->handle.is_finished()) { m_job.reset(); delivery.push_back(job); }
    }
    for (auto& item : delivery) { if (m_closed) break; if (item->handle.status() == SceneStatus::Succeeded) notifications.emit(item->handle.result()); if (m_closed) break; if (item->handle.status() != SceneStatus::Cancelled && item->callback) item->callback(item->handle.result()); }
}
void SceneCenter::shutdown() {
    if (m_closed) return; m_closed = true;
    if (m_job) { m_job->load.cancel(); finish(*m_job, SceneStatus::Cancelled, "SceneCenter closed"); }
    m_job.reset(); m_delivery.clear(); notifications.clear_all();
    if (auto* layer = Object::cast_to<Node>(ObjectDB::get_instance(m_layer))) memdelete(layer);
    m_layer = 0; m_rect = 0;
}
}
