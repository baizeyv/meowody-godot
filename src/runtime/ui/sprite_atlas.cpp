#include "sprite_atlas.h"

namespace meowody {
using namespace godot;

void SpriteAtlas::set_sprites(const Dictionary &value) {
    sprites = value.duplicate();
    lookup = sprites.duplicate();
    Dictionary counts;
    const Array keys = sprites.keys();
    for (int64_t i = 0; i < keys.size(); ++i) {
        const String name = String(keys[i]).get_file().get_basename();
        counts[name] = int64_t(counts.get(name, 0)) + 1;
    }
    for (int64_t i = 0; i < keys.size(); ++i) {
        const String name = String(keys[i]).get_file().get_basename();
        if (int64_t(counts[name]) == 1 && !lookup.has(name)) {
            lookup[name] = sprites[keys[i]];
        }
    }
    emit_changed();
}

Dictionary SpriteAtlas::get_sprites() const {
    return sprites.duplicate();
}

Ref<Texture2D> SpriteAtlas::get_sprite(const String &name) const {
    return lookup.get(name.trim_prefix("res://"), Variant());
}

bool SpriteAtlas::has_sprite(const String &name) const {
    return lookup.has(name.trim_prefix("res://"));
}

PackedStringArray SpriteAtlas::get_sprite_names() const {
    PackedStringArray names;
    const Array keys = sprites.keys();
    for (int64_t i = 0; i < keys.size(); ++i) {
        names.append(keys[i]);
    }
    names.sort();
    return names;
}

}
