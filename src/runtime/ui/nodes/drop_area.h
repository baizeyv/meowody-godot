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
class DropArea : public godot::Control {
    GDREFLECT_CLASS(DropArea, godot::Control)
    GDPROPERTY godot::PackedStringArray allowed_types;
    GDPROPERTY bool drop_enabled = true;
    GDPROPERTY godot::Color highlight_color = godot::Color(0.2, 0.8, 0.6, 0.3);
    GDPROPERTY godot::Callable validator;
    bool highlighted = false;
public:
    DropArea();
    bool _can_drop_data(const godot::Vector2 &at, const godot::Variant &data) const override;
    void _drop_data(const godot::Vector2 &at, const godot::Variant &data) override;
    void _process(double delta) override;
    void _notification(int what);
    GDEMIT_SIGNAL void dropped(godot::Variant payload);
    GDEMIT_SIGNAL void drag_hover_changed(bool accepted);
};
}
