#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <memory>
#include "runtime/core/delegate.hpp"
namespace meowody {
enum class UIStatus { Queued, Loading, Entering, Open, Suspended, Exiting, Closed, Failed, Cancelled };
enum class UIKind { Page, Window, Modal, Overlay };
struct UIOptions {
    UIKind kind = UIKind::Window;
    bool multiple = false, persistent = false, cache = true, dismiss_on_back = true;
    godot::NodePath animation_player;
    godot::StringName enter_animation, exit_animation;
    double animation_timeout = 30.0;
    godot::String pack_url;
};
namespace detail {
struct UIState {
    uint64_t id = 0, root_id = 0;
    UIStatus status = UIStatus::Queued;
    float percent = 0;
    godot::String error;
    bool close_requested = false, front_requested = false;
};
}
class UICenter;
class UIHandle {
    friend class UICenter;
    std::shared_ptr<detail::UIState> state;
    explicit UIHandle(std::shared_ptr<detail::UIState> value) : state(std::move(value)) {}
public:
    UIHandle() = default;
    bool valid() const { return bool(state); }
    uint64_t id() const { return state ? state->id : 0; }
    UIStatus status() const { return state ? state->status : UIStatus::Failed; }
    float progress_percent() const { return state ? state->percent : 0; }
    godot::String error() const { return state ? state->error : godot::String("Invalid UI handle"); }
    godot::Node* root() const { return state ? godot::Object::cast_to<godot::Node>(godot::ObjectDB::get_instance(state->root_id)) : nullptr; }
    bool is_open() const { return status() == UIStatus::Open || status() == UIStatus::Suspended; }
    bool is_finished() const { auto s=status(); return s==UIStatus::Open || s==UIStatus::Suspended || s==UIStatus::Closed || s==UIStatus::Failed || s==UIStatus::Cancelled; }
    void close() const { if (state) state->close_requested = true; }
    void cancel() const { if (state && (status()==UIStatus::Queued || status()==UIStatus::Loading)) { state->close_requested=true; state->status=UIStatus::Cancelled; } }
    void bring_to_front() const { if (state) state->front_requested=true; }
};
using UIOpenCallback = Delegate<void(const UIHandle&)>;
}
