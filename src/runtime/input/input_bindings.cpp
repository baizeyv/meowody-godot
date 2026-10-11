#include "input_state.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_joypad_motion.hpp>
#include <cmath>

using namespace godot;
namespace meowody {
namespace {
SaveStatus invalid(const String &message) { return {SaveError::InvalidArgument, message}; }
bool integer(const Dictionary &data, const char *key, int64_t minimum, int64_t maximum, int64_t &out, int64_t fallback = 0) {
	Variant value = data.get(key, fallback);
	if (value.get_type() != Variant::INT) return false;
	out = value; return out >= minimum && out <= maximum;
}
bool number(const Variant &value, double minimum, double maximum, double &out) {
	if (value.get_type() != Variant::FLOAT && value.get_type() != Variant::INT) return false;
	out = value; return std::isfinite(out) && out >= minimum && out <= maximum;
}
SaveResult encode_event(const Ref<InputEvent> &event) {
	SaveResult result;
	if (event.is_null()) { result.error = SaveError::InvalidArgument; result.message = "Null input binding"; return result; }
	auto &data = result.data;
	data["device"] = event->get_device();
	if (auto key = Object::cast_to<InputEventKey>(event.ptr())) {
		if (key->get_keycode() == KEY_NONE && key->get_physical_keycode() == KEY_NONE && key->get_key_label() == KEY_NONE) {
			result.error = SaveError::InvalidArgument; result.message = "Key binding has no key"; return result;
		}
		data["kind"] = "key"; data["keycode"] = int64_t(key->get_keycode());
		data["physical"] = int64_t(key->get_physical_keycode()); data["label"] = int64_t(key->get_key_label());
		data["unicode"] = int64_t(key->get_unicode()); data["location"] = int64_t(key->get_location());
	} else if (auto mouse = Object::cast_to<InputEventMouseButton>(event.ptr())) {
		data["kind"] = "mouse"; data["button"] = int64_t(mouse->get_button_index());
	} else if (auto button = Object::cast_to<InputEventJoypadButton>(event.ptr())) {
		data["kind"] = "joy_button"; data["button"] = int64_t(button->get_button_index());
	} else if (auto axis = Object::cast_to<InputEventJoypadMotion>(event.ptr())) {
		data["kind"] = "joy_axis"; data["axis"] = int64_t(axis->get_axis()); data["value"] = axis->get_axis_value();
	} else { result.error = SaveError::UnsupportedData; result.message = "Unsupported input binding type"; return result; }
	if (auto mods = Object::cast_to<InputEventWithModifiers>(event.ptr())) {
		data["alt"] = mods->is_alt_pressed(); data["shift"] = mods->is_shift_pressed();
		data["ctrl"] = mods->is_ctrl_pressed(); data["meta"] = mods->is_meta_pressed();
		data["command_or_control"] = mods->is_command_or_control_autoremap();
	}
	return result;
}
SaveStatus decode_event(const Dictionary &data, Ref<InputEvent> &event) {
	Variant kind_value = data.get("kind", Variant());
	if (kind_value.get_type() != Variant::STRING) return invalid("Missing binding kind");
	String kind = kind_value;
	int64_t device;
	if (!integer(data, "device", -1, INT32_MAX, device)) return invalid("Invalid input device");
	if (kind == "key") {
		int64_t code, physical, label, unicode, location;
		if (!integer(data, "keycode", 0, (1 << 23) - 1, code) || !integer(data, "physical", 0, (1 << 23) - 1, physical) ||
			!integer(data, "label", 0, (1 << 23) - 1, label) || !integer(data, "unicode", 0, 0x10ffff, unicode) ||
			!integer(data, "location", 0, 2, location) || (!code && !physical && !label)) return invalid("Invalid key binding");
		Ref<InputEventKey> key; key.instantiate();
		key->set_keycode(Key(code)); key->set_physical_keycode(Key(physical)); key->set_key_label(Key(label));
		key->set_unicode(char32_t(unicode)); key->set_location(KeyLocation(location)); event = key;
	} else if (kind == "mouse") {
		int64_t button;
		if (!integer(data, "button", 1, 9, button)) return invalid("Invalid mouse button");
		Ref<InputEventMouseButton> mouse; mouse.instantiate(); mouse->set_button_index(MouseButton(button)); event = mouse;
	} else if (kind == "joy_button") {
		int64_t button;
		if (!integer(data, "button", 0, 127, button)) return invalid("Invalid joypad button");
		Ref<InputEventJoypadButton> joy; joy.instantiate(); joy->set_button_index(JoyButton(button)); event = joy;
	} else if (kind == "joy_axis") {
		int64_t axis; double value;
		if (!integer(data, "axis", 0, 9, axis) || !number(data.get("value", Variant()), -1, 1, value) || value == 0)
			return invalid("Invalid joypad axis");
		Ref<InputEventJoypadMotion> motion; motion.instantiate(); motion->set_axis(JoyAxis(axis)); motion->set_axis_value(float(value)); event = motion;
	} else return {SaveError::UnsupportedData, "Unknown input binding kind"};
	event->set_device(int(device));
	if (auto mods = Object::cast_to<InputEventWithModifiers>(event.ptr())) {
		for (const char *name : {"alt", "shift", "ctrl", "meta", "command_or_control"})
			if (data.get(name, false).get_type() != Variant::BOOL) return invalid("Invalid input modifier");
		mods->set_alt_pressed(data.get("alt", false)); mods->set_shift_pressed(data.get("shift", false));
		mods->set_ctrl_pressed(data.get("ctrl", false)); mods->set_meta_pressed(data.get("meta", false));
		mods->set_command_or_control_autoremap(data.get("command_or_control", false));
	}
	return {};
}
void capture_default(detail::InputState &state, const StringName &action) {
	if (state.defaults.contains(action)) return;
	auto map = InputMap::get_singleton();
	detail::InputBindingSnapshot snapshot{{}, map->action_get_deadzone(action)};
	auto events = map->action_get_events(action);
	for (int64_t i = 0; i < events.size(); ++i) {
		Ref<InputEvent> event = events[i]; snapshot.events.append(event->duplicate());
	}
	state.defaults.emplace(action, std::move(snapshot));
}
void apply(detail::InputState &state, const StringName &action, const detail::InputBindingSnapshot &binding) {
	capture_default(state, action);
	auto map = InputMap::get_singleton();
	Input::get_singleton()->action_release(action);
	map->action_erase_events(action); map->action_set_deadzone(action, binding.deadzone);
	for (int64_t i = 0; i < binding.events.size(); ++i) map->action_add_event(action, binding.events[i]);
	state.changed();
}
}
SaveStatus InputCenter::rebind(const StringName &action, const TypedArray<InputEvent> &events, float deadzone) {
	if (!state->on_main_thread() || state->closed) return {SaveError::Closed, "InputCenter unavailable on this thread"};
	auto map = InputMap::get_singleton();
	if (!map->has_action(action)) return invalid("Input action does not exist");
	if (deadzone == -1) deadzone = map->action_get_deadzone(action);
	if (!std::isfinite(deadzone) || deadzone < 0 || deadzone >= 1) return invalid("Deadzone must be in [0, 1)");
	detail::InputBindingSnapshot binding{{}, deadzone};
	for (int64_t i = 0; i < events.size(); ++i) {
		auto encoded = encode_event(events[i]);
		if (!encoded.ok()) return {encoded.error, encoded.message};
		Ref<InputEvent> copy;
		auto status = decode_event(encoded.data, copy);
		if (!status.ok()) return status;
		binding.events.append(copy);
	}
	apply(*state, action, binding); return {};
}
bool InputCenter::reset_bindings(const StringName &action) {
	if (!state->on_main_thread() || state->closed || !InputMap::get_singleton()->has_action(action)) return false;
	auto found = state->defaults.find(action);
	if (found == state->defaults.end()) return false;
	apply(*state, action, found->second); return true;
}
SaveResult InputCenter::export_bindings(const PackedStringArray &actions) const {
	SaveResult result;
	if (!state->on_main_thread() || state->closed) { result.error = SaveError::Closed; result.message = "InputCenter closed"; return result; }
	Dictionary bindings;
	auto map = InputMap::get_singleton();
	for (int64_t i = 0; i < actions.size(); ++i) {
		StringName action(actions[i]);
		if (!map->has_action(action)) { result.error = SaveError::InvalidArgument; result.message = "Unknown input action"; return result; }
		Dictionary item; item["deadzone"] = map->action_get_deadzone(action);
		Array events; auto source = map->action_get_events(action);
		for (int64_t j = 0; j < source.size(); ++j) {
			auto encoded = encode_event(source[j]);
			if (!encoded.ok()) { result.error = encoded.error; result.message = encoded.message; return result; }
			events.append(encoded.data);
		}
		item["events"] = events; bindings[String(action)] = item;
	}
	result.data["version"] = 1; result.data["actions"] = bindings;
	return result;
}
SaveStatus InputCenter::import_bindings(const Dictionary &data) {
	if (!state->on_main_thread() || state->closed) return {SaveError::Closed, "InputCenter closed"};
	Variant version = data.get("version", Variant());
	if (version.get_type() != Variant::INT) return invalid("Invalid bindings version");
	if (int64_t(version) != 1) return {SaveError::UnsupportedVersion, "Unsupported bindings version"};
	if (data.get("actions", Variant()).get_type() != Variant::DICTIONARY) return invalid("Invalid actions dictionary");
	Dictionary source = data["actions"];
	std::map<StringName, detail::InputBindingSnapshot> pending;
	auto keys = source.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		Variant key = keys[i];
		if (key.get_type() != Variant::STRING || source[key].get_type() != Variant::DICTIONARY) return invalid("Invalid action record");
		StringName action{String(key)};
		if (!InputMap::get_singleton()->has_action(action)) return invalid("Unknown input action");
		Dictionary item = source[key]; double deadzone;
		if (!number(item.get("deadzone", Variant()), 0, 1, deadzone) || deadzone >= 1 || item.get("events", Variant()).get_type() != Variant::ARRAY)
			return invalid("Invalid action deadzone or events");
		detail::InputBindingSnapshot binding{{}, float(deadzone)}; Array events = item["events"];
		for (int64_t j = 0; j < events.size(); ++j) {
			if (events[j].get_type() != Variant::DICTIONARY) return invalid("Invalid input binding record");
			Ref<InputEvent> event; auto status = decode_event(events[j], event);
			if (!status.ok()) return status;
			binding.events.append(event);
		}
		pending.emplace(action, std::move(binding));
	}
	// Every action is validated before changing any InputMap binding.
	for (const auto &[action, binding] : pending) apply(*state, action, binding);
	return {};
}
} // namespace meowody
