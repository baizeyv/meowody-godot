#include "input_state.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace godot;
namespace meowody {
void detail::InputState::changed() { changed_frame = Engine::get_singleton()->get_process_frames(); }
InputContext::InputContext(std::shared_ptr<detail::InputState> p, uint64_t value) : state(p), id(value) {}
InputContext::~InputContext() { release(); }
InputContext::InputContext(InputContext &&other) noexcept : state(std::move(other.state)), id(std::exchange(other.id, 0)) {}
InputContext &InputContext::operator=(InputContext &&other) noexcept {
	if (this != &other) { release(); state = std::move(other.state); id = std::exchange(other.id, 0); } return *this;
}
bool InputContext::valid() const {
	auto p = state.lock();
	if (!p || !p->on_main_thread() || p->closed) return false;
	return std::any_of(p->active.begin(), p->active.end(), [&](const auto &entry) { return entry.id == id; });
}
void InputContext::release() {
	auto p = state.lock();
	if (p && p->on_main_thread() && !p->closed) {
		const auto count = p->active.size();
		std::erase_if(p->active, [&](const auto &entry) { return entry.id == id; });
		if (p->active.size() != count) p->changed();
	}
	id = 0; state.reset();
}
bool InputContext::set_enabled(bool enabled) {
	auto p = state.lock();
	if (!p || !p->on_main_thread() || p->closed) return false;
	for (auto &entry : p->active) if (entry.id == id) {
		if (entry.enabled != enabled) { entry.enabled = enabled; p->changed(); }
		return true;
	}
	return false;
}
InputCenter::InputCenter() : state(std::make_shared<detail::InputState>()) {}
InputCenter::~InputCenter() { shutdown(); }
bool InputCenter::define_context(const StringName &name, const PackedStringArray &actions, int priority, bool exclusive) {
	if (!state->on_main_thread() || state->closed || name == StringName()) return false;
	for (const auto &entry : state->active) if (entry.name == name) return false;
	detail::InputDefinition definition{{}, priority, exclusive};
	for (int64_t i = 0; i < actions.size(); ++i) {
		StringName action(actions[i]);
		if (!InputMap::get_singleton()->has_action(action)) return false;
		definition.actions.insert(action);
	}
	state->definitions[name] = std::move(definition);
	return true;
}
InputContext InputCenter::push_context(const StringName &name) {
	if (!state->on_main_thread() || state->closed) return {};
	auto found = state->definitions.find(name);
	if (found == state->definitions.end()) return {};
	const auto id = state->next_id++;
	// Activation changes are infrequent; keep priority/latest-first routing ready for polls.
	auto position = std::upper_bound(state->active.begin(), state->active.end(), found->second.priority,
		[](int priority, const auto &entry) { return priority < entry.definition.priority; });
	state->active.insert(position, {id, name, found->second, true}); state->changed();
	return InputContext(state, id);
}
void InputCenter::set_enabled(bool value) {
	if (!state->on_main_thread() || state->closed || state->enabled == value) return;
	state->enabled = value; state->changed();
}
void InputCenter::set_focused(bool value) {
	if (!state->on_main_thread() || state->closed || state->focused == value) return;
	state->focused = value; state->changed();
}
bool InputCenter::can_read(const StringName &context, const StringName &action, bool edge) const {
	if (!state->on_main_thread() || state->closed || !state->enabled || !state->focused) return false;
	if (edge && state->changed_frame == Engine::get_singleton()->get_process_frames()) return false;
	for (auto it = state->active.rbegin(); it != state->active.rend(); ++it) {
		const auto &entry = *it;
		if (!entry.enabled) continue;
		if (entry.name == context) return entry.definition.actions.contains(action);
		if (entry.definition.exclusive || entry.definition.actions.contains(action)) return false;
	}
	return false;
}
bool InputCenter::is_action_pressed(const StringName &c, const StringName &a) const { return can_read(c, a) && Input::get_singleton()->is_action_pressed(a); }
bool InputCenter::is_action_just_pressed(const StringName &c, const StringName &a) const { return can_read(c, a, true) && Input::get_singleton()->is_action_just_pressed(a); }
bool InputCenter::is_action_just_released(const StringName &c, const StringName &a) const { return can_read(c, a, true) && Input::get_singleton()->is_action_just_released(a); }
float InputCenter::get_action_strength(const StringName &c, const StringName &a) const { return can_read(c, a) ? Input::get_singleton()->get_action_strength(a) : 0; }
float InputCenter::get_axis(const StringName &c, const StringName &negative, const StringName &positive) const { return get_action_strength(c, positive) - get_action_strength(c, negative); }
Vector2 InputCenter::get_vector(const StringName &c, const StringName &left, const StringName &right, const StringName &up, const StringName &down, float deadzone) const {
	if (!state->on_main_thread() || state->closed || !std::isfinite(deadzone)) return {};
	auto map = InputMap::get_singleton();
	for (const auto &action : {left, right, up, down}) if (!map->has_action(action)) return {};
	if (deadzone < 0) deadzone = (map->action_get_deadzone(left) + map->action_get_deadzone(right) + map->action_get_deadzone(up) + map->action_get_deadzone(down)) * 0.25f;
	if (deadzone < 0 || deadzone >= 1) return {};
	auto raw = [&](const StringName &a) { return can_read(c, a) ? Input::get_singleton()->get_action_raw_strength(a) : 0.0f; };
	Vector2 value(raw(right) - raw(left), raw(down) - raw(up));
	const auto length = value.length();
	if (length <= deadzone || length == 0) return {};
	return value / length * std::min(1.0f, (float(length) - deadzone) / (1.0f - deadzone));
}
void InputCenter::shutdown() {
	if (!state->on_main_thread()) return;
	state->closed = true; state->active.clear(); state->definitions.clear(); state->defaults.clear();
}
} // namespace meowody
