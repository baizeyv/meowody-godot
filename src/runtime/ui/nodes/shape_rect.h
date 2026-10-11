#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/array.hpp>
namespace meowody {
class ShapeRect : public godot::Control {
    GDREFLECT_CLASS(ShapeRect, godot::Control)
    GDPROPERTY_ENUM("Rounded Rectangle,Capsule,Circle,Chamfer") int shape = 0;
    GDPROPERTY godot::Vector4 corner_radii = godot::Vector4(12, 12, 12, 12);
    GDPROPERTY godot::Color fill_color = godot::Color(0.2, 0.5, 0.9);
    GDPROPERTY godot::Ref<godot::Gradient> gradient;
    GDPROPERTY double gradient_angle = 90;
    GDPROPERTY_RANGE(0, 100, 0.5) double border_width = 0;
    GDPROPERTY godot::Color border_color = godot::Color(1, 1, 1);
    GDPROPERTY godot::Color shadow_color = godot::Color(0, 0, 0, 0.3);
    GDPROPERTY godot::Vector2 shadow_offset = godot::Vector2(0, 4);
    GDPROPERTY_RANGE(0, 100, 0.5) double shadow_blur = 8;
    GDPROPERTY_RANGE(0, 100, 0.5) double shadow_spread = 0;
    godot::Ref<godot::ShaderMaterial> shader_material;
    godot::Ref<godot::GradientTexture1D> ramp;
    godot::Array state;
    double padding = 0;
public:
    ShapeRect();
    void _validate_property(godot::PropertyInfo &property) const;
    void _process(double delta) override;
    void _notification(int what);
};
}
