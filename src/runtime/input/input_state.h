#pragma once
#include "input_center.h"
#include <map>
#include <set>
#include <thread>
#include <vector>

namespace meowody::detail {
struct InputDefinition { std::set<godot::StringName> actions; int priority; bool exclusive; };
struct ActiveInputContext {
	uint64_t id;
	godot::StringName name;
	InputDefinition definition;
	bool enabled = true;
};
struct InputBindingSnapshot { godot::TypedArray<godot::InputEvent> events; float deadzone; };
struct InputState {
	const std::thread::id owner = std::this_thread::get_id();
	std::map<godot::StringName, InputDefinition> definitions;
	std::vector<ActiveInputContext> active; // Ascending priority, then activation id; polls read backwards.
	std::map<godot::StringName, InputBindingSnapshot> defaults;
	uint64_t next_id = 1;
	uint64_t changed_frame = 0;
	bool enabled = true, focused = true, closed = false;
	bool on_main_thread() const { return owner == std::this_thread::get_id(); }
	void changed();
};
} // namespace meowody::detail
