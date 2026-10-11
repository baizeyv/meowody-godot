#include "drag_area.h"
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
#include <godot_cpp/variant/callable.hpp>
namespace meowody {
using namespace godot;
DragArea::DragArea() { set_process(false); set_mouse_filter(MOUSE_FILTER_STOP); }
Dictionary DragArea::get_drag_payload() const { Dictionary result; result["meowody_drag"] = true; result["type"] = drag_type; result["data"] = data; result["source_id"] = int64_t(get_instance_id()); return result; }
Control *DragArea::make_ghost() {
    Control *ghost = nullptr;
    if (ghost_scene.is_valid()) { Node *node = ghost_scene->instantiate(); ghost = Object::cast_to<Control>(node); if (!ghost && node) memdelete(node); }
    if (!ghost && ghost_texture.is_valid()) { auto *rect = memnew(TextureRect); rect->set_texture(ghost_texture); rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE); ghost = rect; }
    if (!ghost) { auto *label = memnew(Label); label->set_text(String(drag_type).is_empty() ? "Drag" : String(drag_type)); ghost = label; }
    ghost->set_size(get_size()); ghost->set_mouse_filter(MOUSE_FILTER_IGNORE); ghost->set_modulate(Color(1,1,1,0.75)); return ghost;
}
void DragArea::_gui_input(const Ref<InputEvent> &event) {
    if (editor() || !drag_enabled) return;
    Ref<InputEventMouseButton> button = event;
    if (button.is_valid() && button->get_button_index() == MOUSE_BUTTON_LEFT) { pending = button->is_pressed(); origin = button->get_position(); }
    Ref<InputEventMouseMotion> motion = event;
    if (motion.is_valid() && pending && !dragging && origin.distance_to(motion->get_position()) >= std::max(0.0,finite(drag_threshold))) {
        pending = false; dragging = true; set_process(true);
        force_drag(get_drag_payload(), make_ghost()); drag_started(data); accept_event();
    }
}
void DragArea::_process(double) { if (dragging) last_pointer = get_global_mouse_position(); if (!drag_enabled || !is_visible_in_tree()) pending = false; }
void DragArea::_notification(int what) {
    if (what == NOTIFICATION_READY) set_process(false);
    if (what == NOTIFICATION_EXIT_TREE || what == NOTIFICATION_WM_WINDOW_FOCUS_OUT) pending = false;
    if (what != NOTIFICATION_DRAG_END || !dragging) return;
    dragging = false; set_process(false); bool success = is_drag_successful();
    if (!success && snap_back && is_inside_tree() && is_visible_in_tree()) {
        Control *ghost = make_ghost(); add_child(ghost, false, INTERNAL_MODE_BACK); ghost->set_as_top_level(true); ghost->set_global_position(last_pointer);
        Ref<Tween> tween = create_tween(); tween->tween_property(ghost,"global_position",get_global_position(),0.18);
        tween->tween_callback(Callable(ghost, "queue_free"));
    }
    drag_finished(success);
}
}
