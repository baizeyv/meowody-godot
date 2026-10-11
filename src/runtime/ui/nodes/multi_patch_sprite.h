#pragma once

#include "reflection/reflection.h"
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>

namespace meowody {

class MultiPatchSprite : public godot::Resource {
    GDREFLECT_CLASS(MultiPatchSprite, godot::Resource)
private:
    GDPROPERTY_ACCESSORS(get_texture, set_texture)
    godot::Ref<godot::Texture2D> texture;
    GDPROPERTY_ACCESSORS(get_region, set_region)
    godot::Rect2i region;
    GDPROPERTY_ACCESSORS(get_x_cuts, set_x_cuts)
    godot::PackedInt32Array x_cuts;
    GDPROPERTY_ACCESSORS(get_y_cuts, set_y_cuts)
    godot::PackedInt32Array y_cuts;
    GDPROPERTY_ACCESSORS(get_x_modes, set_x_modes)
    godot::PackedInt32Array x_modes;
    GDPROPERTY_ACCESSORS(get_y_modes, set_y_modes)
    godot::PackedInt32Array y_modes;
    GDPROPERTY_ACCESSORS(get_x_weights, set_x_weights)
    godot::PackedFloat32Array x_weights;
    GDPROPERTY_ACCESSORS(get_y_weights, set_y_weights)
    godot::PackedFloat32Array y_weights;
    void texture_changed();
public:
    // Serialized mode values are stable. Empty modes use Fixed outer / Stretch inner.
    enum PatchMode { FIXED, STRETCH, TILE, TILE_FIT };
    struct Patch { godot::Rect2 source, destination; };
    ~MultiPatchSprite() override;
    void set_texture(const godot::Ref<godot::Texture2D> &value);
    godot::Ref<godot::Texture2D> get_texture() const { return texture; }
    void set_region(const godot::Rect2i &value);
    godot::Rect2i get_region() const { return region; }
    void set_x_cuts(const godot::PackedInt32Array &value);
    godot::PackedInt32Array get_x_cuts() const { return x_cuts; }
    void set_y_cuts(const godot::PackedInt32Array &value);
    godot::PackedInt32Array get_y_cuts() const { return y_cuts; }
    void set_x_modes(const godot::PackedInt32Array &value);
    godot::PackedInt32Array get_x_modes() const { return x_modes; }
    void set_y_modes(const godot::PackedInt32Array &value);
    godot::PackedInt32Array get_y_modes() const { return y_modes; }
    void set_x_weights(const godot::PackedFloat32Array &value);
    godot::PackedFloat32Array get_x_weights() const { return x_weights; }
    void set_y_weights(const godot::PackedFloat32Array &value);
    godot::PackedFloat32Array get_y_weights() const { return y_weights; }

    GDMETHOD
    godot::Rect2i get_effective_region() const;
    GDMETHOD
    godot::Vector2 get_minimum_size() const;
    GDMETHOD
    godot::String get_validation_error() const;
    GDMETHOD
    godot::Array get_draw_regions(const godot::Vector2 &size) const;

    std::vector<Patch> layout(const godot::Vector2 &size) const;
    void draw_to(const godot::RID &canvas, const godot::Rect2 &rect, const godot::Color &tint,
                 bool transpose = false, const godot::Rect2 &clip = godot::Rect2()) const;
};

}
