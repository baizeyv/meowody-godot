#include "multi_patch_rect.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>
namespace meowody {
using namespace godot;
MultiPatchRect::~MultiPatchRect() {
    if (sprite.is_valid()) sprite->disconnect("changed", callable_mp(this, &MultiPatchRect::sprite_changed));
}
void MultiPatchRect::sprite_changed() { update_minimum_size(); queue_redraw(); }
void MultiPatchRect::set_sprite(const Ref<MultiPatchSprite> &value) {
    if (sprite == value) return;
    if (sprite.is_valid()) sprite->disconnect("changed", callable_mp(this, &MultiPatchRect::sprite_changed));
    sprite = value;
    if (sprite.is_valid()) sprite->connect("changed", callable_mp(this, &MultiPatchRect::sprite_changed));
    sprite_changed();
}
Vector2 MultiPatchRect::_get_minimum_size() const { return sprite.is_valid() ? sprite->get_minimum_size() : Vector2(); }
void MultiPatchRect::_notification(int what) {
    if (what == NOTIFICATION_DRAW && sprite.is_valid()) sprite->draw_to(get_canvas_item(), Rect2(Vector2(), get_size()), Color(1, 1, 1, 1));
    if (what == NOTIFICATION_RESIZED) queue_redraw();
}
}
