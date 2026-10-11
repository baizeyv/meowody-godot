#pragma once
#include <godot_cpp/variant/string.hpp>
#include <memory>
namespace meowody {
enum class SceneStatus { Loading, FadingOut, Switching, FadingIn, Succeeded, Failed, Cancelled };
struct SceneOptions { double fade_out_seconds = 0.25, fade_in_seconds = 0.25; };
struct SceneResult {
    bool succeeded = false;
    godot::String message;
    uint64_t scene_id = 0;
};
namespace detail {
struct SceneState { SceneStatus status = SceneStatus::Loading; SceneResult result; double progress = 0; bool cancel = false, committed = false; };
}
class SceneCenter;
class SceneHandle {
public:
    bool valid() const { return bool(m_state); }
    SceneStatus status() const { return m_state ? m_state->status : SceneStatus::Cancelled; }
    bool is_finished() const { auto s = status(); return s == SceneStatus::Succeeded || s == SceneStatus::Failed || s == SceneStatus::Cancelled; }
    double progress_percent() const { return is_finished() ? 100 : (m_state ? m_state->progress : 0); }
    SceneResult result() const { return m_state ? m_state->result : SceneResult{}; }
    bool cancel() { if (!m_state || m_state->committed || is_finished()) return false; m_state->cancel = true; return true; }
private:
    friend class SceneCenter;
    std::shared_ptr<detail::SceneState> m_state;
};
}
