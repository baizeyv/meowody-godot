#include "repeat_button.h"
#include "node_helpers.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
namespace meowody {
using namespace godot;
RepeatButton::RepeatButton() {
    set_process(false);
    connect("button_down", callable_mp(this, &RepeatButton::begin_hold));
    connect("button_up", callable_mp(this, &RepeatButton::cancel_hold));
    connect("mouse_exited", callable_mp(this, &RepeatButton::cancel_hold));
    connect("focus_exited", callable_mp(this, &RepeatButton::cancel_hold));
}
RepeatButton::~RepeatButton() {
    // Godot keeps the node/signals while hot reload replaces this native instance.
    const auto begin = callable_mp(this, &RepeatButton::begin_hold);
    const auto cancel = callable_mp(this, &RepeatButton::cancel_hold);
    if (is_connected("button_down", begin)) disconnect("button_down", begin);
    if (is_connected("button_up", cancel)) disconnect("button_up", cancel);
    if (is_connected("mouse_exited", cancel)) disconnect("mouse_exited", cancel);
    if (is_connected("focus_exited", cancel)) disconnect("focus_exited", cancel);
}
void RepeatButton::begin_hold() { if (editor() || is_disabled()) return; held = true; set_process(true); elapsed = 0; repeats = 0; until_repeat = std::max(0.0,finite(initial_delay)); if (trigger_on_press) repeated(++repeats); }
void RepeatButton::cancel_hold() { held = false; set_process(false); }
void RepeatButton::_notification(int what) {
    if (what == NOTIFICATION_READY) set_process(false); if (what == NOTIFICATION_WM_WINDOW_FOCUS_OUT || what == NOTIFICATION_EXIT_TREE) cancel_hold(); }
void RepeatButton::_process(double delta) {
    if (!held) return;
    if (is_disabled() || !is_visible_in_tree()) { cancel_hold(); return; }
    elapsed += delta; until_repeat -= delta;
    // A stalled frame must not cause an unbounded burst of purchases/actions.
    for (int i = 0; held && until_repeat <= 0 && i < 8; ++i) {
        repeated(++repeats);
        if (is_disabled() || is_queued_for_deletion()) { cancel_hold(); break; }
        until_repeat += std::max(std::max(0.01,finite(minimum_interval)), finite(repeat_interval,0.15) / (1 + std::max(0.0,finite(acceleration)) * elapsed));
    }
    until_repeat = std::max(0.0,until_repeat);
}
}
