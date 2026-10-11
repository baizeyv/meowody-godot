#pragma once
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <memory>
namespace meowody {
enum class VoiceStatus { Playing, Paused, Finished, Stopped, Failed };
struct AudioChannelOptions {
    float volume = 1;
    bool muted = false, paused = false;
    size_t max_voices = 16;
    bool steal_oldest = true;
    godot::String bus = "Master";
};
struct AudioPlayOptions { float volume = 1, pitch = 1, from_seconds = 0; };
namespace detail {
struct VoiceState {
    uint64_t id = 0;
    VoiceStatus status = VoiceStatus::Failed;
    float volume = 1;
    bool paused = false;
};
}
class AudioCenter;
class AudioHandle {
public:
    uint64_t id() const { return m_state ? m_state->id : 0; }
    VoiceStatus status() const { return m_state ? m_state->status : VoiceStatus::Failed; }
    bool is_finished() const { auto s = status(); return s == VoiceStatus::Finished || s == VoiceStatus::Stopped || s == VoiceStatus::Failed; }
    void stop();
    void pause();
    void resume();
    bool set_volume(float value);
    float position_seconds() const;
    bool seek(float seconds);
private:
    friend class AudioCenter;
    std::shared_ptr<detail::VoiceState> m_state;
};
}
