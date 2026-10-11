#include "arc_progress.h"
#include <godot_cpp/classes/atlas_texture.hpp>
#include <vector>
#include <godot_cpp/classes/rendering_server.hpp>
#include "node_helpers.h"
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
namespace meowody {
using namespace godot;
static constexpr double PI = 3.141592653589793;
ArcProgress::ArcProgress() { set_process(true); set_mouse_filter(MOUSE_FILTER_IGNORE); }
ArcProgress::~ArcProgress() {
    if (observed.is_valid()) observed->disconnect("changed", callable_mp(this, &ArcProgress::gradient_changed));
    if (observed_texture.is_valid()) observed_texture->disconnect("changed", callable_mp(this, &ArcProgress::gradient_changed));
}
void ArcProgress::gradient_changed() { queue_redraw(); }
void ArcProgress::_process(double) {
    if (observed_texture != texture) {
        if (observed_texture.is_valid()) observed_texture->disconnect("changed", callable_mp(this, &ArcProgress::gradient_changed));
        observed_texture = texture;
        if (observed_texture.is_valid()) observed_texture->connect("changed", callable_mp(this, &ArcProgress::gradient_changed));
        queue_redraw();
    }
    if (observed != gradient) {
        if (observed.is_valid()) observed->disconnect("changed", callable_mp(this, &ArcProgress::gradient_changed));
        observed = gradient;
        if (observed.is_valid()) observed->connect("changed", callable_mp(this, &ArcProgress::gradient_changed));
        queue_redraw();
    }
    if (changed(state, {value, start_angle, end_angle, clockwise, thickness, rounded_caps, color, track_color, texture, get_size()})) queue_redraw();
}
void ArcProgress::_notification(int what) {
    if (what != NOTIFICATION_DRAW) return;
    Vector2 size = get_size(), center = size * 0.5;
    double outer = std::min(size.x, size.y) * 0.5, width = std::clamp(finite(thickness, 1), 0.0, outer);
    double radius = outer - width * 0.5;
    if (radius <= 0 || width <= 0) return;
    double begin = finite(start_angle) * PI / 180, sweep = std::clamp(std::abs(finite(end_angle) - finite(start_angle)), 0.0, 360.0) * PI / 180 * (clockwise ? 1 : -1);
    // Polygon drawing uses a page RID. Resolve AtlasTexture logical coordinates and
    // clip against trimmed margins before submitting UVs, including nested atlases.
    Ref<Texture2D> page = texture;
    Vector2 texture_size = texture.is_valid() ? texture->get_size() : Vector2(1,1), offset;
    Rect2 visible(Vector2(), texture_size);
    for (Ref<AtlasTexture> atlas = page; atlas.is_valid(); atlas = page) {
        Rect2 region = atlas->get_region();
        if (atlas->get_atlas().is_null()) { page.unref(); break; }
        if (region.size == Vector2()) region.size = atlas->get_atlas()->get_size();
        visible = visible.intersection(Rect2(atlas->get_margin().position - offset, region.size));
        offset += region.position - atlas->get_margin().position;
        page = atlas->get_atlas();
    }
    PackedVector2Array batch_points, batch_uv; PackedColorArray batch_colors; PackedInt32Array batch_indices;
    struct Vertex { Vector2 point; Color color; };
    // A convex quad clipped to a rectangle has at most eight vertices.
    std::vector<Vertex> vertices, clipped;
    vertices.reserve(8); clipped.reserve(8);
    const Vector2 page_size = page.is_valid() ? page->get_size() : Vector2(1,1);
    const Rect2 clip = texture_size.x > 0 && texture_size.y > 0 ?
        Rect2(visible.position / texture_size * size, visible.size / texture_size * size) : Rect2();
    auto paint = [&](double fraction, bool foreground) {
        double span = sweep * fraction;
        int steps = std::clamp(int(std::ceil(std::abs(span) * radius / 3)), 2, 512);
        bool textured = foreground && page.is_valid() && texture_size.x > 0 && texture_size.y > 0;
        int max_vertices = textured ? 8 : 4;
        batch_points.resize(steps * max_vertices); batch_uv.resize(steps * max_vertices);
        batch_colors.resize(steps * max_vertices); batch_indices.resize(steps * (max_vertices - 2) * 3);
        auto *points = batch_points.ptrw(); auto *uv = batch_uv.ptrw();
        auto *colors = batch_colors.ptrw(); auto *indices = batch_indices.ptrw();
        int point_count = 0, index_count = 0;
        for (int i = 0; i < steps; ++i) {
            double a = begin + span * i / steps, b = begin + span * (i + 1) / steps;
            vertices.clear();
            Vector2 at_a(std::cos(a), std::sin(a)), at_b(std::cos(b), std::sin(b));
            Color color_a = foreground ? (gradient.is_valid() ? gradient->sample(sweep == 0 ? 0 : (a - begin) / sweep) * color : color) : track_color;
            Color color_b = foreground ? (gradient.is_valid() ? gradient->sample(sweep == 0 ? 0 : (b - begin) / sweep) * color : color) : track_color;
            vertices.push_back({center + at_a * outer, color_a});
            vertices.push_back({center + at_a * (outer - width), color_a});
            vertices.push_back({center + at_b * (outer - width), color_b});
            vertices.push_back({center + at_b * outer, color_b});
            if (textured) for (int edge=0;edge<4;++edge) {
                clipped.clear();
                int axis = edge / 2; bool lower = edge % 2 == 0;
                double bound = lower ? clip.position[axis] : clip.get_end()[axis];
                auto inside = [&](const Vertex &v) { return lower ? v.point[axis] >= bound : v.point[axis] <= bound; };
                for (size_t j=0;j<vertices.size();++j) {
                    auto start=vertices[j], end=vertices[(j+1)%vertices.size()];
                    bool ia=inside(start), ib=inside(end);
                    if (ia) clipped.push_back(start);
                    if (ia != ib) { double t=(bound-start.point[axis])/(end.point[axis]-start.point[axis]); clipped.push_back({start.point.lerp(end.point,t), start.color.lerp(end.color,t)}); }
                }
                vertices.swap(clipped);
            }
            if (vertices.size()<3) continue;
            int base = point_count;
            for (const auto &vertex : vertices) {
                points[point_count] = vertex.point; colors[point_count] = vertex.color;
                uv[point_count++] = textured ? (vertex.point / size * texture_size + offset) / page_size : Vector2();
            }
            for (int j=1;j+1<int(vertices.size());++j) {
                indices[index_count++] = base; indices[index_count++] = base+j; indices[index_count++] = base+j+1;
            }
        }
        batch_points.resize(point_count); batch_uv.resize(point_count);
        batch_colors.resize(point_count); batch_indices.resize(index_count);
        if (!batch_indices.is_empty()) RenderingServer::get_singleton()->canvas_item_add_triangle_array(
            get_canvas_item(), batch_indices, batch_points, batch_colors, batch_uv, PackedInt32Array(), PackedFloat32Array(),
            foreground && page.is_valid() ? page->get_rid() : RID());
        if (rounded_caps && std::abs(span) > 0.0001 && std::abs(span) < PI * 2 - 0.001) {
            draw_circle(center + Vector2(std::cos(begin), std::sin(begin)) * radius, width * 0.5, foreground ? (gradient.is_valid() ? gradient->sample(0) * color : color) : track_color);
            double end = begin + span;
            draw_circle(center + Vector2(std::cos(end), std::sin(end)) * radius, width * 0.5, foreground ? (gradient.is_valid() ? gradient->sample(fraction) * color : color) : track_color);
        }
    };
    paint(1, false);
    if (unit(value) > 0) paint(unit(value), true);
}
}
