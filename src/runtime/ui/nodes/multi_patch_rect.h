#pragma once
#include "multi_patch_sprite.h"
#include <godot_cpp/classes/control.hpp>

namespace meowody {
class MultiPatchRect : public godot::Control {
    GDREFLECT_CLASS(MultiPatchRect, godot::Control)
private:
    GDPROPERTY_ACCESSORS(get_sprite, set_sprite)
    godot::Ref<MultiPatchSprite> sprite;
    void sprite_changed();
public:
    ~MultiPatchRect() override;
    void set_sprite(const godot::Ref<MultiPatchSprite> &value);
    godot::Ref<MultiPatchSprite> get_sprite() const { return sprite; }
    godot::Vector2 _get_minimum_size() const override;
    void _notification(int what);
};
}
