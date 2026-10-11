#include "shape_rect.h"
#include "node_helpers.h"
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
namespace meowody {
using namespace godot;
static constexpr double PI = 3.141592653589793;
ShapeRect::ShapeRect() {
    set_mouse_filter(MOUSE_FILTER_IGNORE); set_process(true); ramp.instantiate();
    shader_material.instantiate();
    Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://addons/meowody_nodes/shaders/shape_rect.gdshader");
    shader_material->set_shader(shader); set_material(shader_material);
}
void ShapeRect::_validate_property(PropertyInfo &property) const { if (property.name == StringName("material")) property.usage = PROPERTY_USAGE_NONE; }
void ShapeRect::_process(double) {
    if (!changed(state, {get_size(), shape, corner_radii, fill_color, gradient, gradient_angle, border_width, border_color, shadow_color, shadow_offset, shadow_blur, shadow_spread})) return;
    padding = std::max(std::abs(shadow_offset.x), std::abs(shadow_offset.y)) + std::clamp(finite(shadow_blur),0.0,100.0) + std::clamp(finite(shadow_spread),0.0,100.0) + 2;
    ramp->set_gradient(gradient);
    shader_material->set_shader_parameter("ramp", ramp);
    shader_material->set_shader_parameter("use_gradient", gradient.is_valid());
    shader_material->set_shader_parameter("box_size", get_size());
    shader_material->set_shader_parameter("padding", padding);
    shader_material->set_shader_parameter("shape", std::clamp(shape,0,3));
    shader_material->set_shader_parameter("radii", corner_radii);
    shader_material->set_shader_parameter("fill_color", fill_color);
    shader_material->set_shader_parameter("border_color", border_color);
    shader_material->set_shader_parameter("border_width", std::max(0.0,finite(border_width)));
    shader_material->set_shader_parameter("shadow_color", shadow_color);
    shader_material->set_shader_parameter("shadow_offset", shadow_offset);
    shader_material->set_shader_parameter("shadow_blur", std::clamp(finite(shadow_blur),0.0,100.0));
    shader_material->set_shader_parameter("shadow_spread", std::clamp(finite(shadow_spread),0.0,100.0));
    shader_material->set_shader_parameter("angle", finite(gradient_angle) * PI / 180);
    queue_redraw();
}
void ShapeRect::_notification(int what) { if (what == NOTIFICATION_DRAW) draw_rect(Rect2(Vector2(-padding,-padding), get_size()+Vector2(padding*2,padding*2)),Color(1,1,1)); }}
