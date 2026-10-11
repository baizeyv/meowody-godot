#pragma once

#include "reflection/reflection.h"

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace meowody {

// Generated atlas catalog. All sprite textures share their packed page resources.
class SpriteAtlas : public godot::Resource {
    GDREFLECT_CLASS(SpriteAtlas, godot::Resource)

private:
    GDPROPERTY_USAGE(godot::PROPERTY_USAGE_STORAGE)
    GDPROPERTY_ACCESSORS(get_sprites, set_sprites)
    godot::Dictionary sprites;
    godot::Dictionary lookup;

public:
    void set_sprites(const godot::Dictionary &value);
    godot::Dictionary get_sprites() const;

    GDMETHOD
    godot::Ref<godot::Texture2D> get_sprite(const godot::String &name) const;

    GDMETHOD
    bool has_sprite(const godot::String &name) const;

    GDMETHOD
    godot::PackedStringArray get_sprite_names() const;
};

}
