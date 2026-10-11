#pragma once
#include "multi_patch_sprite.h"

namespace meowody {
class MultiPatchTexture : public godot::Texture2D {
    GDREFLECT_CLASS(MultiPatchTexture, godot::Texture2D)
private:
    GDPROPERTY_ACCESSORS(get_sprite, set_sprite)
    godot::Ref<MultiPatchSprite> sprite;
    void sprite_changed();
public:
    ~MultiPatchTexture() override;
    void set_sprite(const godot::Ref<MultiPatchSprite> &value);
    godot::Ref<MultiPatchSprite> get_sprite() const { return sprite; }
    int32_t _get_width() const override;
    int32_t _get_height() const override;
    bool _has_alpha() const override { return true; }
    void _draw(const godot::RID &canvas, const godot::Vector2 &position, const godot::Color &tint, bool transpose) const override;
    void _draw_rect(const godot::RID &canvas, const godot::Rect2 &rect, bool tile, const godot::Color &tint, bool transpose) const override;
    void _draw_rect_region(const godot::RID &canvas, const godot::Rect2 &rect, const godot::Rect2 &source, const godot::Color &tint, bool transpose, bool clip_uv) const override;
};
}
