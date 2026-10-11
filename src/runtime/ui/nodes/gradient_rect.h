#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/array.hpp>
namespace meowody {
class GradientRect : public godot::Control {
    GDREFLECT_CLASS(GradientRect, godot::Control)
    GDPROPERTY godot::Ref<godot::Gradient> gradient;
    GDPROPERTY_ENUM("Linear,Radial,Conic") int gradient_mode = 0;
    GDPROPERTY double angle = 0;
    GDPROPERTY godot::Vector2 center = godot::Vector2(0.5, 0.5);
    GDPROPERTY_RANGE(0.001, 2, 0.01) double radius = 0.5;
    godot::Ref<godot::ShaderMaterial> shader_material;
    godot::Ref<godot::GradientTexture1D> ramp;
    godot::Array state;
public:
    GradientRect();
    void _validate_property(godot::PropertyInfo &property) const;
    void _process(double delta) override;
    void _notification(int what);
};
}
