#pragma once
#include "runtime/save/save_types.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <memory>

namespace meowody {
namespace detail { struct InputState; }
class InputContext {
	std::weak_ptr<detail::InputState> state;
	uint64_t id = 0;
	InputContext(std::shared_ptr<detail::InputState> state, uint64_t id);
	friend class InputCenter;
public:
	InputContext() = default;
	~InputContext();
	InputContext(const InputContext &) = delete;
	InputContext &operator=(const InputContext &) = delete;
	InputContext(InputContext &&other) noexcept;
	InputContext &operator=(InputContext &&other) noexcept;
	bool valid() const;
	bool set_enabled(bool enabled);
	void release();
};
class InputCenter {
	std::shared_ptr<detail::InputState> state;
	bool can_read(const godot::StringName &context, const godot::StringName &action, bool edge = false) const;
public:
	InputCenter();
	~InputCenter();
	InputCenter(const InputCenter &) = delete;
	InputCenter &operator=(const InputCenter &) = delete;
	bool define_context(const godot::StringName &name, const godot::PackedStringArray &actions, int priority = 0, bool exclusive = true);
	InputContext push_context(const godot::StringName &name);
	void set_enabled(bool enabled);
	void set_focused(bool focused);
	bool is_action_pressed(const godot::StringName &context, const godot::StringName &action) const;
	bool is_action_just_pressed(const godot::StringName &context, const godot::StringName &action) const;
	bool is_action_just_released(const godot::StringName &context, const godot::StringName &action) const;
	float get_action_strength(const godot::StringName &context, const godot::StringName &action) const;
	float get_axis(const godot::StringName &context, const godot::StringName &negative, const godot::StringName &positive) const;
	godot::Vector2 get_vector(const godot::StringName &context, const godot::StringName &left, const godot::StringName &right,
		const godot::StringName &up, const godot::StringName &down, float deadzone = -1.0f) const;
	SaveStatus rebind(const godot::StringName &action, const godot::TypedArray<godot::InputEvent> &events, float deadzone = -1.0f);
	bool reset_bindings(const godot::StringName &action);
	SaveResult export_bindings(const godot::PackedStringArray &actions) const;
	SaveStatus import_bindings(const godot::Dictionary &data);
	void shutdown();
};
} // namespace meowody
