#include "audio_center.h"
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/object.hpp>
#include <cmath>
#include <algorithm>
#include <vector>
namespace meowody {
using namespace godot;
static AudioStreamPlayer* player(uint64_t id) { return Object::cast_to<AudioStreamPlayer>(ObjectDB::get_instance(id)); }
AudioCenter::AudioCenter() {
    create_channel("Music"); create_channel("SFX"); create_channel("UI"); create_channel("Voice");
}
AudioCenter::~AudioCenter() { shutdown(); }
bool AudioCenter::ensure_root() {
    if (ObjectDB::get_instance(m_root)) return true;
    auto* tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (!tree || !tree->get_root()) return false;
    auto* root = memnew(Node); root->set_name("MeowodyAudio"); root->set_process_mode(Node::PROCESS_MODE_ALWAYS);
    tree->get_root()->add_child(root); m_root = root->get_instance_id(); return true;
}
bool AudioCenter::create_channel(const String& name, AudioChannelOptions options) {
    if (m_closed || name.is_empty() || has_channel(name) || !std::isfinite(options.volume) || options.volume < 0 || options.volume > 1 ||
        !options.max_voices || options.bus.is_empty()) return false;
    Channel c; c.options = options; m_channels.emplace(name, std::move(c)); return true;
}
bool AudioCenter::has_channel(const String& name) const { return m_channels.contains(name); }
PackedStringArray AudioCenter::channels() const { PackedStringArray a; for (auto& [n, c] : m_channels) a.append(n); return a; }
AudioChannelOptions AudioCenter::channel_options(const String& name) const { auto it = m_channels.find(name); return it == m_channels.end() ? AudioChannelOptions{} : it->second.options; }
bool AudioCenter::set_volume(const String& name, float volume) {
    auto it = m_channels.find(name); if (m_closed || it == m_channels.end() || !std::isfinite(volume) || volume < 0 || volume > 1) return false;
    it->second.duration = 0; it->second.options.volume = volume;
    for (auto& [id, v] : m_voices) if (v->channel == name) apply(*v); return true;
}
bool AudioCenter::set_muted(const String& name, bool muted) {
    auto it = m_channels.find(name); if (m_closed || it == m_channels.end()) return false;
    it->second.options.muted = muted; for (auto& [id, v] : m_voices) if (v->channel == name) apply(*v); return true;
}
bool AudioCenter::set_paused(const String& name, bool paused) {
    auto it = m_channels.find(name); if (m_closed || it == m_channels.end()) return false;
    it->second.options.paused = paused; for (auto& [id, v] : m_voices) if (v->channel == name) apply(*v); return true;
}
bool AudioCenter::fade_volume(const String& name, float target, double seconds) {
    auto it = m_channels.find(name); if (m_closed || it == m_channels.end() || !std::isfinite(target) || target < 0 || target > 1 || !std::isfinite(seconds) || seconds < 0) return false;
    if (!seconds) return set_volume(name, target);
    auto& c = it->second; c.from = c.options.volume; c.target = target; c.elapsed = 0; c.duration = seconds; return true;
}
void AudioCenter::apply(Voice& voice, AudioStreamPlayer* p) {
    if (!p) p = player(voice.handle.id());
    auto it = m_channels.find(voice.channel);
    if (!p || it == m_channels.end() || voice.handle.is_finished()) return;
    auto& c = it->second.options; auto& state = *voice.handle.m_state;
    const float volume = c.muted ? 0 : c.volume * state.volume;
    if (voice.applied_volume != volume || p->get_volume_db() != voice.applied_db) {
        p->set_volume_linear(volume);
        voice.applied_volume = volume; voice.applied_db = p->get_volume_db();
    }
    const bool paused = c.paused || state.paused;
    if (p->get_stream_paused() != paused) p->set_stream_paused(paused);
    state.status = paused ? VoiceStatus::Paused : VoiceStatus::Playing;
}
AudioHandle AudioCenter::play(const String& channel, const Ref<AudioStream>& stream, AudioPlayOptions options, Delegate<void()> finished) {
    AudioHandle handle; auto c = m_channels.find(channel);
    if (m_closed || c == m_channels.end() || stream.is_null() || !std::isfinite(options.volume) || options.volume < 0 || options.volume > 1 ||
        !std::isfinite(options.pitch) || options.pitch <= 0 || !std::isfinite(options.from_seconds) || options.from_seconds < 0 || !ensure_root()) return handle;
    if (!Engine::get_singleton()->has_singleton("AudioServer") || AudioServer::get_singleton()->get_bus_index(c->second.options.bus) < 0) return handle;
    if (active_count(channel) >= c->second.options.max_voices) {
        if (!c->second.options.steal_oldest) return handle;
        for (auto& [id, v] : m_voices) if (v->channel == channel && !v->handle.is_finished()) { v->handle.stop(); break; }
    }
    auto* p = memnew(AudioStreamPlayer); p->set_stream(stream); p->set_bus(c->second.options.bus); p->set_pitch_scale(options.pitch);
    Object::cast_to<Node>(ObjectDB::get_instance(m_root))->add_child(p);
    handle.m_state = std::make_shared<detail::VoiceState>(); handle.m_state->id = p->get_instance_id();
    handle.m_state->status = VoiceStatus::Playing; handle.m_state->volume = options.volume;
    auto voice = std::make_shared<Voice>(); voice->handle = handle; voice->channel = channel; voice->callback = std::move(finished);
    m_voices.emplace(handle.id(), voice); p->play(options.from_seconds); apply(*voice); return handle;
}
size_t AudioCenter::active_count(const String& channel) const {
    size_t n = 0; for (auto& [id, v] : m_voices) if (v->channel == channel && !v->handle.is_finished() && player(id)) ++n; return n;
}
void AudioCenter::stop_channel(const String& channel) { for (auto& [id, v] : m_voices) if (v->channel == channel) v->handle.stop(); }
void AudioCenter::stop_all() { for (auto& [id, v] : m_voices) v->handle.stop(); }
bool AudioCenter::remove_channel(const String& name) { if (m_closed || !has_channel(name)) return false; stop_channel(name); m_channels.erase(name); return true; }
void AudioCenter::update(double delta) {
    if (m_closed || m_updating) return;
    m_updating = true; struct Guard { bool& v; ~Guard() { v = false; } } guard{m_updating};
    if (!std::isfinite(delta) || delta < 0) delta = 0;
    for (auto& [name, c] : m_channels) if (c.duration > 0) {
        c.elapsed = std::min(c.duration, c.elapsed + delta); c.options.volume = c.from + (c.target - c.from) * (c.elapsed / c.duration);
        if (c.elapsed >= c.duration) c.duration = 0;
    }
    std::vector<std::shared_ptr<Voice>> done;
    for (auto it = m_voices.begin(); it != m_voices.end();) {
        auto& voice = *it->second; auto* p = player(it->first);
        if (!p && !voice.handle.is_finished()) voice.handle.m_state->status = VoiceStatus::Stopped;
        if (p && !voice.handle.is_finished()) {
            apply(voice, p);
            if (!p->is_playing() && voice.handle.status() != VoiceStatus::Paused) voice.handle.m_state->status = VoiceStatus::Finished;
        }
        if (voice.handle.is_finished()) { if (p) p->queue_free(); done.push_back(it->second); it = m_voices.erase(it); }
        else ++it;
    }
    for (auto& v : done) { if (m_closed) break; if (v->handle.status() == VoiceStatus::Finished && v->callback) v->callback(); }
}
void AudioCenter::shutdown() {
    if (m_closed) return; m_closed = true; stop_all(); m_voices.clear(); m_channels.clear();
    if (auto* root = Object::cast_to<Node>(ObjectDB::get_instance(m_root))) memdelete(root);
    m_root = 0;
}
}
