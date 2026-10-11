#include "drop_area.h"
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
DropArea::DropArea() { set_process(false); set_mouse_filter(MOUSE_FILTER_STOP); }
bool DropArea::_can_drop_data(const Vector2 &, const Variant &value) const {
    if (!drop_enabled || value.get_type() != Variant::DICTIONARY) return false;
    Dictionary payload = value;
    if (!bool(payload.get("meowody_drag",false))) return false;
    String type = payload.get("type",String());
    if (!allowed_types.is_empty() && !allowed_types.has(type)) return false;
    return !validator.is_valid() || bool(validator.call(payload.get("data",Variant())));
}
void DropArea::_drop_data(const Vector2 &at, const Variant &payload) { if (_can_drop_data(at,payload)) { Dictionary value = payload; dropped(value.get("data",Variant())); } }
void DropArea::_process(double) {
    if (editor()) return;
    bool next = get_viewport()->gui_is_dragging() && Rect2(Vector2(),get_size()).has_point(get_local_mouse_position()) && _can_drop_data(get_local_mouse_position(),get_viewport()->gui_get_drag_data());
    if (next != highlighted) { highlighted = next; queue_redraw(); drag_hover_changed(next); }
}
void DropArea::_notification(int what) {
    if (what == NOTIFICATION_READY) set_process(false);
    if (what == NOTIFICATION_DRAG_BEGIN && !editor()) set_process(true);
    if (what == NOTIFICATION_DRAG_END) {
        set_process(false);
        if (highlighted) { highlighted = false; queue_redraw(); drag_hover_changed(false); }
    }
    if (what == NOTIFICATION_DRAW && highlighted) draw_rect(Rect2(Vector2(),get_size()),highlight_color);
}
}
