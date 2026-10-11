#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/array.hpp>
namespace meowody {
class ArcProgress : public godot::Control {
    GDREFLECT_CLASS(ArcProgress, godot::Control)
    GDPROPERTY_RANGE(0, 1, 0.001) double value = 0.75;
    GDPROPERTY double start_angle = -90;
    GDPROPERTY double end_angle = 270;
    GDPROPERTY bool clockwise = true;
    GDPROPERTY_RANGE(1, 200, 0.5) double thickness = 12;
    GDPROPERTY bool rounded_caps = true;
    GDPROPERTY godot::Color color = godot::Color(0.3, 0.7, 1);
    GDPROPERTY godot::Color track_color = godot::Color(0.15, 0.18, 0.24);
    GDPROPERTY godot::Ref<godot::Gradient> gradient;
    GDPROPERTY godot::Ref<godot::Texture2D> texture;
    godot::Array state;
    godot::Ref<godot::Gradient> observed;
    godot::Ref<godot::Texture2D> observed_texture;
    void gradient_changed();
public:
    ArcProgress();
    ~ArcProgress() override;
    void _process(double delta) override;
    void _notification(int what);
};
}
