#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <map>
namespace meowody {
class DragArea : public godot::Control {
    GDREFLECT_CLASS(DragArea, godot::Control)
    GDPROPERTY godot::Variant data;
    GDPROPERTY godot::StringName drag_type;
    GDPROPERTY godot::Ref<godot::Texture2D> ghost_texture;
    GDPROPERTY godot::Ref<godot::PackedScene> ghost_scene;
    GDPROPERTY_RANGE(0, 100, 1) double drag_threshold = 8;
    GDPROPERTY bool snap_back = true;
    GDPROPERTY bool drag_enabled = true;
    bool pending = false, dragging = false;
    godot::Vector2 origin, last_pointer;
    godot::Control *make_ghost();
public:
    DragArea();
    void _gui_input(const godot::Ref<godot::InputEvent> &event) override;
    void _process(double delta) override;
    void _notification(int what);
    GDMETHOD godot::Dictionary get_drag_payload() const;
    GDEMIT_SIGNAL void drag_started(godot::Variant payload);
    GDEMIT_SIGNAL void drag_finished(bool success);
};
}
