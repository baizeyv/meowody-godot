#include "multi_patch_texture.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/variant/callable_method_pointer.hpp>
namespace meowody {
using namespace godot;
MultiPatchTexture::~MultiPatchTexture() {
    if (sprite.is_valid()) sprite->disconnect("changed", callable_mp(this, &MultiPatchTexture::sprite_changed));
}
void MultiPatchTexture::sprite_changed() { emit_changed(); }
void MultiPatchTexture::set_sprite(const Ref<MultiPatchSprite> &value) {
    if (sprite == value) return;
    if (sprite.is_valid()) sprite->disconnect("changed", callable_mp(this, &MultiPatchTexture::sprite_changed));
    sprite = value;
    if (sprite.is_valid()) sprite->connect("changed", callable_mp(this, &MultiPatchTexture::sprite_changed));
    emit_changed();
}
int32_t MultiPatchTexture::_get_width() const { return sprite.is_valid() ? sprite->get_effective_region().size.x : 0; }
int32_t MultiPatchTexture::_get_height() const { return sprite.is_valid() ? sprite->get_effective_region().size.y : 0; }
void MultiPatchTexture::_draw(const RID &canvas, const Vector2 &position, const Color &tint, bool transpose) const {
    Vector2 size(_get_width(), _get_height());
    if (transpose) std::swap(size.x, size.y);
    _draw_rect(canvas, Rect2(position, size), false, tint, transpose);
}
void MultiPatchTexture::_draw_rect(const RID &canvas, const Rect2 &rect, bool tile, const Color &tint, bool transpose) const {
    // Segment modes own tiling; a consumer's whole-texture tile flag is intentionally ignored.
    if (sprite.is_valid()) sprite->draw_to(canvas, rect, tint, transpose);
}
void MultiPatchTexture::_draw_rect_region(const RID &canvas, const Rect2 &rect, const Rect2 &source, const Color &tint, bool transpose, bool clip_uv) const {
    if (sprite.is_null() || source.size.x <= 0 || source.size.y <= 0) return;
    Vector2 source_size = source.size, source_position = source.position, native_size(_get_width(), _get_height());
    if (transpose) { std::swap(source_size.x, source_size.y); std::swap(source_position.x, source_position.y); std::swap(native_size.x, native_size.y); }
    Vector2 bounds(std::abs(rect.size.x), std::abs(rect.size.y));
    Vector2 scale = bounds / source_size;
    if (rect.size.x < 0) source_position.x = native_size.x - source_position.x - source_size.x;
    if (rect.size.y < 0) source_position.y = native_size.y - source_position.y - source_size.y;
    Vector2 full_size = native_size * scale;
    if (rect.size.x < 0) full_size.x = -full_size.x;
    if (rect.size.y < 0) full_size.y = -full_size.y;
    sprite->draw_to(canvas, Rect2(rect.position - source_position * scale, full_size), tint, transpose, Rect2(rect.position, bounds));
}
}
