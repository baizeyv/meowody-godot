#include "gesture_area.h"
#include <godot_cpp/classes/time.hpp>
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
GestureArea::GestureArea() { set_process(false); set_mouse_filter(MOUSE_FILTER_STOP); }
void GestureArea::cancel_gestures() { touches.clear(); set_process(false); last_tap_time = -100; }
void GestureArea::_notification(int what) {
    if (what == NOTIFICATION_READY) set_process(false); if (what == NOTIFICATION_WM_WINDOW_FOCUS_OUT || what == NOTIFICATION_EXIT_TREE || (what == NOTIFICATION_VISIBILITY_CHANGED && !is_visible_in_tree())) cancel_gestures(); }
void GestureArea::begin(int id, Vector2 position) {
    if (touches.size() >= 10 || touches.count(id)) return;
    clock = Time::get_singleton()->get_ticks_msec() / 1000.0; set_process(true);
    touches[id] = {position,position,clock};
    if (touches.size() > 1) for (auto &entry : touches) entry.second.multi = true;
}
void GestureArea::move(int id, Vector2 position) {
    auto it = touches.find(id); if (it == touches.end()) return;
    Vector2 before_pair;
    if (touches.size() >= 2) { auto a = touches.begin(), b = std::next(a); before_pair = b->second.position - a->second.position; }
    Vector2 delta = position - it->second.position; it->second.position = position;
    if (position.distance_to(it->second.start) > std::max(0.0,finite(movement_threshold))) it->second.moved = true;
    if (touches.size() == 1 && it->second.moved) pan(delta);
    if (touches.size() >= 2) {
        auto a = touches.begin(), b = std::next(a); Vector2 after = b->second.position - a->second.position;
        if (before_pair.length() > 0.001 && after.length() > 0.001) { pinch(after.length() / before_pair.length()); rotate(before_pair.angle_to(after)); }
    }
}
void GestureArea::end(int id, Vector2 position, bool canceled) {
    auto it = touches.find(id); if (it == touches.end()) return;
    clock = Time::get_singleton()->get_ticks_msec() / 1000.0;
    Touch touch = it->second; touches.erase(it);
    if (touches.empty()) set_process(false);
    if (canceled || touch.multi || touch.long_fired) return;
    Vector2 displacement = position-touch.start; double duration = clock-touch.time;
    if (displacement.length() >= std::max(1.0,finite(swipe_distance)) && duration <= std::max(0.01,finite(swipe_time))) swipe(displacement.normalized());
    else if (!touch.moved && displacement.length() <= std::max(0.0,finite(movement_threshold))) {
        if (clock-last_tap_time <= std::max(0.01,finite(double_tap_time)) && position.distance_to(last_tap_position) <= std::max(1.0,finite(movement_threshold))) { double_tap(position); last_tap_time = -100; }
        else { tap(position); last_tap_time = clock; last_tap_position = position; }
    }
}
void GestureArea::_gui_input(const Ref<InputEvent> &event) {
    if (editor()) return;
    Ref<InputEventScreenTouch> touch = event;
    if (touch.is_valid()) { if (touch->is_pressed()) begin(touch->get_index(),touch->get_position()); else end(touch->get_index(),touch->get_position(),touch->is_canceled()); accept_event(); return; }
    Ref<InputEventScreenDrag> drag = event;
    if (drag.is_valid()) { move(drag->get_index(),drag->get_position()); accept_event(); return; }
    if (!mouse_enabled || (!touches.empty() && !touches.count(-1))) return;
    Ref<InputEventMouseButton> button = event;
    if (button.is_valid() && button->get_button_index() == MOUSE_BUTTON_LEFT) { if (button->is_pressed()) begin(-1,button->get_position()); else end(-1,button->get_position(),button->is_canceled()); accept_event(); }
    Ref<InputEventMouseMotion> motion = event;
    if (motion.is_valid() && touches.count(-1)) { move(-1,motion->get_position()); accept_event(); }
}
void GestureArea::_process(double delta) {
    clock = Time::get_singleton()->get_ticks_msec() / 1000.0;
    if (touches.size() != 1) return;
    Touch &touch = touches.begin()->second;
    if (!touch.multi && !touch.moved && !touch.long_fired && clock-touch.time >= std::max(0.01,finite(long_press_time))) { touch.long_fired = true; long_press(touch.position); }
}}
