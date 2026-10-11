#pragma once
#include "reflection/reflection.h"
#include "runtime/ui/nodes/multi_patch_texture.h"
#include "runtime/ui/sprite_atlas.h"
#include <array>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/array.hpp>
namespace meowody {
class SegmentProgressBar : public godot::Control {
	GDREFLECT_CLASS(SegmentProgressBar, godot::Control)
	GDPROPERTY_RANGE(1, 4096, 1)
	int segments = 5;
	GDPROPERTY double value = 3.5;
	GDPROPERTY_RANGE(0, 100, 0.5)
	double gap = 4;
	GDPROPERTY_ENUM("Left to Right,Right to Left,Bottom to Top,Top to Bottom")
	int direction = 0;
	GDPROPERTY bool partial_fill = true;
	GDPROPERTY godot::Color background_color = godot::Color(0.15, 0.18, 0.24);
	GDPROPERTY godot::Color fill_color = godot::Color(0.2, 0.8, 0.6);
	GDPROPERTY godot::Ref<godot::Texture2D> background_texture;
	GDPROPERTY godot::Ref<godot::Texture2D> fill_texture;
	GDPROPERTY godot::Ref<godot::Texture2D> separator_texture;
	GDPROPERTY godot::Ref<meowody::SpriteAtlas> sprite_atlas;
	GDPROPERTY godot::StringName background_sprite;
	GDPROPERTY godot::StringName fill_sprite;
	GDPROPERTY godot::StringName separator_sprite;
	GDPROPERTY godot::Ref<meowody::MultiPatchSprite> background_patch;
	GDPROPERTY godot::Ref<meowody::MultiPatchSprite> fill_patch;
	GDPROPERTY godot::Ref<meowody::MultiPatchSprite> separator_patch;
	godot::Array state, source_state;
	std::array<godot::Ref<godot::Texture2D>, 3> resolved;
	std::array<godot::Ref<MultiPatchTexture>, 3> patches;
	godot::Ref<SpriteAtlas> observed_atlas;
	bool sources_dirty = true;
	void atlas_changed();
	std::array<godot::Ref<godot::Texture2D>, 3> observed;
	void texture_changed();

public:
	SegmentProgressBar();
	~SegmentProgressBar() override;
	void _process(double delta) override;
	void _notification(int what);
	GDMETHOD double get_fill_ratio() const;
};
} //namespace meowody
