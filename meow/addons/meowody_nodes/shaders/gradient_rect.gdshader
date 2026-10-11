shader_type canvas_item;
uniform sampler2D ramp : source_color, filter_linear, repeat_disable;
uniform int mode = 0; uniform float angle = 0.0; uniform vec2 center = vec2(0.5); uniform float radius = 0.5;
void fragment() {
 vec2 p = UV - center; vec2 axis = vec2(cos(angle), sin(angle));
 float t = mode == 0 ? dot(p, axis) + 0.5 : (mode == 1 ? length(p) / max(radius, 0.001) : fract((atan(p.y,p.x)-angle)/6.2831853));
 COLOR *= texture(ramp, vec2(clamp(t,0.0,1.0),0.5));
}
