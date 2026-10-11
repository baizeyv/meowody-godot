#include "audio_types.h"
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/core/object.hpp>
#include <cmath>
namespace meowody {
static godot::AudioStreamPlayer* player(uint64_t id) { return godot::Object::cast_to<godot::AudioStreamPlayer>(godot::ObjectDB::get_instance(id)); }
void AudioHandle::stop() {
    if (is_finished()) return;
    m_state->status = VoiceStatus::Stopped;
    if (auto* p = player(id())) p->stop();
}
void AudioHandle::pause() {
    if (is_finished()) return;
    m_state->paused = true; m_state->status = VoiceStatus::Paused;
    if (auto* p = player(id())) p->set_stream_paused(true);
}
void AudioHandle::resume() { if (!is_finished()) m_state->paused = false; }
bool AudioHandle::set_volume(float value) {
    if (is_finished() || !std::isfinite(value) || value < 0 || value > 1) return false;
    m_state->volume = value; return true;
}
float AudioHandle::position_seconds() const { auto* p = player(id()); return p ? p->get_playback_position() : 0; }
bool AudioHandle::seek(float seconds) {
    auto* p = player(id()); if (!p || is_finished() || !std::isfinite(seconds) || seconds < 0) return false;
    p->seek(seconds); return true;
}
}
