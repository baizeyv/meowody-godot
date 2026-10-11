#pragma once
#include "audio_types.h"
#include "runtime/core/delegate.hpp"
#include <map>
#include <godot_cpp/variant/packed_string_array.hpp>
namespace godot { class AudioStreamPlayer; }
namespace meowody {
class AudioCenter {
public:
    AudioCenter();
    ~AudioCenter();
    AudioCenter(const AudioCenter&) = delete;
    AudioCenter& operator=(const AudioCenter&) = delete;
    bool create_channel(const godot::String& name, AudioChannelOptions options = {});
    bool remove_channel(const godot::String& name);
    bool has_channel(const godot::String& name) const;
    godot::PackedStringArray channels() const;
    bool set_volume(const godot::String& channel, float volume);
    bool set_muted(const godot::String& channel, bool muted);
    bool set_paused(const godot::String& channel, bool paused);
    AudioChannelOptions channel_options(const godot::String& channel) const;
    bool fade_volume(const godot::String& channel, float target, double seconds);
    AudioHandle play(const godot::String& channel, const godot::Ref<godot::AudioStream>& stream,
        AudioPlayOptions options = {}, Delegate<void()> finished = {});
    void stop_channel(const godot::String& channel);
    void stop_all();
    size_t active_count(const godot::String& channel) const;
    void update(double delta);
    void shutdown();
    bool is_closed() const { return m_closed; }
private:
    struct Channel { AudioChannelOptions options; double duration = 0, elapsed = 0; float from = 1, target = 1; };
    struct Voice {
        AudioHandle handle; godot::String channel; Delegate<void()> callback;
        float applied_volume = -1, applied_db = 0;
    };
    std::map<godot::String, Channel> m_channels;
    std::map<uint64_t, std::shared_ptr<Voice>> m_voices;
    uint64_t m_root = 0;
    bool m_closed = false, m_updating = false;
    bool ensure_root();
    void apply(Voice& voice, godot::AudioStreamPlayer* player = nullptr);
};
}
