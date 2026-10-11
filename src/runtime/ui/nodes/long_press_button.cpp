#include "long_press_button.h"
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
LongPressButton::LongPressButton() {
    set_process(false);
    connect("button_down", callable_mp(this, &LongPressButton::begin_hold));
    connect("button_up", callable_mp(this, &LongPressButton::cancel_hold));
    connect("mouse_exited", callable_mp(this, &LongPressButton::cancel_hold));
    connect("focus_exited", callable_mp(this, &LongPressButton::cancel_hold));
}
LongPressButton::~LongPressButton() {
    const auto begin = callable_mp(this, &LongPressButton::begin_hold);
    const auto cancel = callable_mp(this, &LongPressButton::cancel_hold);
    if (is_connected("button_down", begin)) disconnect("button_down", begin);
    if (is_connected("button_up", cancel)) disconnect("button_up", cancel);
    if (is_connected("mouse_exited", cancel)) disconnect("mouse_exited", cancel);
    if (is_connected("focus_exited", cancel)) disconnect("focus_exited", cancel);
}
void LongPressButton::begin_hold() { if (editor() || is_disabled()) return; held = true; set_process(true); fired = false; elapsed = 0; origin = get_local_mouse_position(); long_press_progress(0); }
void LongPressButton::cancel_hold() { if (held) { held = false; set_process(false); elapsed = 0; long_press_progress(0); } }
void LongPressButton::_notification(int what) {
    if (what == NOTIFICATION_READY) set_process(false); if (what == NOTIFICATION_WM_WINDOW_FOCUS_OUT || what == NOTIFICATION_EXIT_TREE) cancel_hold(); }
void LongPressButton::_process(double delta) {
    if (!held) return;
    if (is_disabled() || !is_visible_in_tree() || (Input::get_singleton()->is_mouse_button_pressed(MOUSE_BUTTON_LEFT) && origin.distance_to(get_local_mouse_position()) > std::max(0.0,finite(cancel_distance)))) { cancel_hold(); return; }
    if (fired && !repeat) return;
    elapsed += delta; double duration = std::max(0.01, finite(long_press_time,0.5));
    long_press_progress(unit(elapsed / duration));
    if (elapsed >= duration) { fired = true; elapsed = 0; long_pressed(); }
}
}
