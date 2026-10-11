#include "gradient_rect.h"
#include "node_helpers.h"
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
namespace meowody {
using namespace godot;
static constexpr double PI = 3.141592653589793;
GradientRect::GradientRect() {
    set_mouse_filter(MOUSE_FILTER_IGNORE); set_process(true);
    ramp.instantiate();
    shader_material.instantiate();
    Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://addons/meowody_nodes/shaders/gradient_rect.gdshader");
    shader_material->set_shader(shader); set_material(shader_material);
}
void GradientRect::_validate_property(PropertyInfo &property) const { if (property.name == StringName("material")) property.usage = PROPERTY_USAGE_NONE; }
void GradientRect::_process(double) {
    if (!changed(state, {gradient, gradient_mode, angle, center, radius})) return;
    ramp->set_gradient(gradient); shader_material->set_shader_parameter("ramp", ramp);
    shader_material->set_shader_parameter("mode", std::clamp(gradient_mode, 0, 2));
    shader_material->set_shader_parameter("angle", finite(angle) * PI / 180);
    shader_material->set_shader_parameter("center", center);
    shader_material->set_shader_parameter("radius", std::max(0.001, finite(radius, 0.5)));
    queue_redraw();
}
void GradientRect::_notification(int what) { if (what == NOTIFICATION_READY && gradient.is_null()) gradient.instantiate(); if (what == NOTIFICATION_DRAW) draw_rect(Rect2(Vector2(), get_size()), Color(1,1,1)); }
}
