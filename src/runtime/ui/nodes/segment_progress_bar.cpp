#include "segment_progress_bar.h"
#include "node_helpers.h"
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
namespace meowody {
using namespace godot;
SegmentProgressBar::SegmentProgressBar() { set_process(true); set_mouse_filter(MOUSE_FILTER_IGNORE); }
SegmentProgressBar::~SegmentProgressBar() {
    if (observed_atlas.is_valid()) observed_atlas->disconnect("changed", callable_mp(this, &SegmentProgressBar::atlas_changed));
    for (auto image : observed) if (image.is_valid() && image->is_connected("changed", callable_mp(this, &SegmentProgressBar::texture_changed))) image->disconnect("changed", callable_mp(this, &SegmentProgressBar::texture_changed));
}
void SegmentProgressBar::atlas_changed() { sources_dirty = true; queue_redraw(); }
void SegmentProgressBar::texture_changed() { queue_redraw(); }
double SegmentProgressBar::get_fill_ratio() const { return unit(value / std::max(1, segments)); }
void SegmentProgressBar::_process(double) {
    if (observed_atlas != sprite_atlas) {
        if (observed_atlas.is_valid()) observed_atlas->disconnect("changed", callable_mp(this, &SegmentProgressBar::atlas_changed));
        observed_atlas = sprite_atlas;
        if (observed_atlas.is_valid()) observed_atlas->connect("changed", callable_mp(this, &SegmentProgressBar::atlas_changed));
        sources_dirty = true;
    }
    if (changed(source_state, {sprite_atlas, background_sprite, fill_sprite, separator_sprite, background_patch, fill_patch, separator_patch, background_texture, fill_texture, separator_texture})) sources_dirty = true;
    if (sources_dirty) {
        std::array<Ref<Texture2D>,3> fallback = {background_texture,fill_texture,separator_texture};
        std::array<Ref<MultiPatchSprite>,3> data = {background_patch,fill_patch,separator_patch};
        std::array<StringName,3> names = {background_sprite,fill_sprite,separator_sprite};
        for (int i=0;i<3;++i) {
            resolved[i] = fallback[i];
            if (sprite_atlas.is_valid() && !names[i].is_empty()) resolved[i] = sprite_atlas->get_sprite(String(names[i]));
            if (data[i].is_valid()) {
                if (patches[i].is_null()) patches[i].instantiate();
                patches[i]->set_sprite(data[i]); resolved[i] = patches[i];
            } else patches[i].unref();
        }
        sources_dirty = false; queue_redraw();
    }
    const auto &textures = resolved;
    if (textures != observed) {
        for (auto image : observed) if (image.is_valid() && image->is_connected("changed", callable_mp(this, &SegmentProgressBar::texture_changed))) image->disconnect("changed", callable_mp(this, &SegmentProgressBar::texture_changed));
        observed = textures;
        for (auto image : observed) if (image.is_valid() && !image->is_connected("changed", callable_mp(this, &SegmentProgressBar::texture_changed))) image->connect("changed", callable_mp(this, &SegmentProgressBar::texture_changed));
    }
    if (changed(state, {segments, value, gap, direction, partial_fill, background_color, fill_color, background_texture, fill_texture, separator_texture, get_size()})) queue_redraw();
}
void SegmentProgressBar::_notification(int what) {
    if (what != NOTIFICATION_DRAW) return;
    const auto &background_texture = resolved[0];
    const auto &fill_texture = resolved[1];
    const auto &separator_texture = resolved[2];
    int count = std::clamp(segments, 1, 4096), mode = std::clamp(direction, 0, 3);
    bool vertical = mode >= 2, reverse = mode == 1 || mode == 2;
    Vector2 size = get_size();
    double extent = vertical ? size.y : size.x;
    double spacing = std::clamp(finite(gap), 0.0, extent / std::max(1, count - 1));
    double length = std::max(0.0, (extent - spacing * (count - 1)) / count);
    const Vector2 fill_size = fill_texture.is_valid() ? fill_texture->get_size() : Vector2(1, 1);
    for (int i = 0; i < count; ++i) {
        double pos = (reverse ? count - 1 - i : i) * (length + spacing);
        Rect2 rect = vertical ? Rect2(0, pos, size.x, length) : Rect2(pos, 0, length, size.y);
        if (background_texture.is_valid()) background_texture->draw_rect(get_canvas_item(), rect, false, background_color);
        else draw_rect(rect, background_color);
        double fraction = unit(finite(value) - i);
        if (!partial_fill) fraction = fraction >= 1 ? 1 : 0;
        Rect2 fill = rect, source(Vector2(), fill_size);
        int axis = vertical ? 1 : 0;
        fill.size[axis] *= fraction;
        source.size[axis] *= fraction;
        if (reverse) { fill.position[axis] += rect.size[axis] * (1 - fraction); if (fill_texture.is_valid()) source.position[axis] = fill_size[axis] * (1 - fraction); }
        if (fraction > 0) {
            if (fill_texture.is_valid()) fill_texture->draw_rect_region(get_canvas_item(), fill, source, fill_color);
            else draw_rect(fill, fill_color);
        }
        if (separator_texture.is_valid() && i < count - 1 && spacing > 0) {
            double at = (i + 1) * length + i * spacing;
            separator_texture->draw_rect(get_canvas_item(), vertical ? Rect2(0, at, size.x, spacing) : Rect2(at, 0, spacing, size.y), false);
        }
    }
}
}
