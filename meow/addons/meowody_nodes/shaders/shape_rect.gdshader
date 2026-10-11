shader_type canvas_item;
uniform vec2 box_size; uniform float padding; uniform int shape;
uniform vec4 radii; uniform vec4 fill_color : source_color; uniform vec4 border_color : source_color;
uniform float border_width; uniform vec4 shadow_color : source_color; uniform vec2 shadow_offset;
uniform float shadow_blur; uniform float shadow_spread;
uniform sampler2D ramp : source_color, filter_linear, repeat_disable; uniform bool use_gradient; uniform float angle;
float sdf(vec2 p) {
 vec2 half_size = box_size * 0.5;
 if (shape == 2) return length(p) - min(half_size.x, half_size.y);
 float r = p.y < 0.0 ? (p.x < 0.0 ? radii.x : radii.y) : (p.x < 0.0 ? radii.w : radii.z);
 r = clamp(r,0.0,min(half_size.x,half_size.y));
 if (shape == 1) r = min(half_size.x,half_size.y);
 vec2 q = abs(p) - half_size;
 if (shape == 3) return max(max(q.x,q.y), (q.x+q.y+r)*0.70710678);
 q += r; return length(max(q,vec2(0.0))) + min(max(q.x,q.y),0.0) - r;
}
void fragment() {
 vec2 p = UV * (box_size+2.0*padding) - padding - box_size*0.5;
 float d = sdf(p); float aa = max(fwidth(d),0.75);
 float coverage = 1.0-smoothstep(-aa,aa,d);
 float inside = 1.0-smoothstep(-aa,aa,d+border_width);
 vec4 fill = fill_color;
 if (use_gradient) fill *= texture(ramp,vec2(clamp(dot(p / max(box_size,vec2(1.0)),vec2(cos(angle),sin(angle)))+0.5,0.0,1.0),0.5));
 vec4 foreground = mix(border_color,fill,inside); foreground.a *= coverage;
 float shadow = 1.0-smoothstep(-aa,max(aa,shadow_blur),sdf(p-shadow_offset)-shadow_spread);
 float alpha = foreground.a + shadow_color.a*shadow*(1.0-foreground.a);
 vec3 rgb = (foreground.rgb*foreground.a+shadow_color.rgb*shadow_color.a*shadow*(1.0-foreground.a))/max(alpha,0.00001);
 COLOR *= vec4(rgb,alpha);
}
