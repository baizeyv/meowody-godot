#include "debug_overlay.h"

#include <algorithm>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include "runtime/app.h"

namespace meowody {
using namespace godot;
void DebugGraph::_draw() {
	draw_rect(Rect2(Vector2(), get_size()), Color(.035, .045, .065));
	if (values.size() < 2)
		return;
	float top = 34;
	for (auto v : values)
		top = std::max(top, v.y);
	Vector2 previous;
	for (size_t i = 0; i < values.size(); ++i) {
		Vector2 p(float(i) * get_size().x / 239,
				get_size().y * (1 - values[i].y / top));
		if (i)
			draw_line(previous, p, Color(.2, .85, .8), 2);
		previous = p;
	}
	draw_line(Vector2(0, get_size().y * (1 - 16.67 / top)),
			Vector2(get_size().x, get_size().y * (1 - 16.67 / top)),
			Color(.7, .5, .2));
}
void DebugOverlay::setup(DebugCenter *owner) {
	center = owner;
	set_layer(1000);
	set_process_mode(Node::PROCESS_MODE_ALWAYS);
	root = memnew(Control);
	add_child(root);
	root->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	hud = memnew(PanelContainer);
	root->add_child(hud);
	hud->set_position(Vector2(std::max(0.f, root->get_size().x - 250), 12));
	auto *row = memnew(HBoxContainer);
	hud->add_child(row);
	row->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	fps = memnew(Label);
	row->add_child(fps);
	fps->set_text("FPS -- | -- ms");
	fps->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	auto *expand = memnew(Button);
	expand->set_text("Debug [F12]");
	row->add_child(expand);
	expand->connect("pressed", callable_mp(this, &DebugOverlay::toggle));
	hud->connect("gui_input", callable_mp(this, &DebugOverlay::hud_input));
	modal = memnew(PanelContainer);
	root->add_child(modal);
	modal->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	modal->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	modal->hide();
	auto *box = memnew(VBoxContainer);
	modal->add_child(box);
	auto *title = memnew(HBoxContainer);
	box->add_child(title);
	auto *label = memnew(Label);
	label->set_text("MEOWODY  /  DEBUG CONSOLE");
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title->add_child(label);
	auto *close = memnew(Button);
	close->set_text("Collapse [Esc]");
	title->add_child(close);
	close->connect("pressed", callable_mp(this, &DebugOverlay::toggle));
	tabs = memnew(TabContainer);
	tabs->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	box->add_child(tabs);
	auto *page = memnew(VBoxContainer);
	page->set_name("Overview");
	tabs->add_child(page);
	overview = memnew(Label);
	page->add_child(overview);
	graph = memnew(DebugGraph);
	graph->set_custom_minimum_size(Vector2(0, 140));
	graph->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page->add_child(graph);
	auto *legend = memnew(Label);
	legend->set_text("Frame time (ms), last 60 s / gold: 16.67 ms");
	page->add_child(legend);
	log_panel = memnew(DebugLogPanel);
	log_panel->set_name("Logs");
	tabs->add_child(log_panel);
	log_panel->setup(&center->logs);
	auto *terminal = memnew(VBoxContainer);
	terminal->set_name("Console");
	tabs->add_child(terminal);
	console = memnew(TextEdit);
	console->set_editable(false);
	console->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	terminal->add_child(console);
	command = memnew(LineEdit);
	command->set_placeholder(
			"help | Enter: run | Up/Down: history | Tab: completion");
	terminal->add_child(command);
	command->connect("text_submitted",
			callable_mp(this, &DebugOverlay::submit));
	build_inspection_pages();
	set_process_input(true);
}
void DebugOverlay::show_modal(bool value) {
	if (expanded == value || !modal)
		return;
	expanded = value;
	if (value) {
		if (auto *focus = root->get_viewport()->gui_get_focus_owner())
			focus_id = uint64_t(focus->get_instance_id());
		modal->show();
		hud->hide();
		modal->set_focus_mode(Control::FOCUS_ALL);
		modal->grab_focus();
	} else {
		modal->hide();
		hud->show();
		if (auto *focus = Object::cast_to<Control>(
					ObjectDB::get_instance(ObjectID(focus_id))))
			if (focus->is_inside_tree() && focus->is_visible_in_tree())
				focus->grab_focus();
		focus_id = 0;
	}
}
void DebugOverlay::toggle() {
	if (center)
		center->toggle();
}
void DebugOverlay::submit(const String &text) {
	if (center && center->execute_command(text)) {
		command->clear();
		history_index = -1;
	}
}
void DebugOverlay::_input(const Ref<InputEvent> &event) {
	if (!center || !center->is_enabled())
		return;
	Ref<InputEventKey> key = event;
	if (key.is_null() || !key->is_pressed() || key->is_echo())
		return;
	auto code = key->get_keycode();
	if (code == center->toggle_key) {
		center->toggle();
		root->get_viewport()->set_input_as_handled();
		return;
	}
	if (!center->is_open())
		return;
	if (code == KEY_ESCAPE) {
		center->close();
		root->get_viewport()->set_input_as_handled();
		return;
	}
	if (!command->has_focus())
		return;
	if (code == KEY_UP || code == KEY_DOWN) {
		int size = int(center->history.size());
		if (size) {
			if (history_index < 0)
				history_index = size;
			history_index = std::clamp(
					history_index + (code == KEY_UP ? -1 : 1), 0, size);
			command->set_text(history_index == size
							? String()
							: center->history[history_index]);
			command->set_caret_column(command->get_text().length());
		}
		root->get_viewport()->set_input_as_handled();
	}
	if (code == KEY_TAB) {
		auto matches = center->complete(command->get_text());
		if (matches.size() == 1) {
			command->set_text(matches[0] + " ");
			command->set_caret_column(command->get_text().length());
		} else if (matches.size())
			center->print(String("  ").join(matches));
		root->get_viewport()->set_input_as_handled();
	}
}
void DebugOverlay::hud_input(const Ref<InputEvent> &event) {
	Ref<InputEventMouseButton> button = event;
	Ref<InputEventMouseMotion> motion = event;
	Ref<InputEventScreenTouch> touch = event;
	Ref<InputEventScreenDrag> drag = event;
	if (button.is_valid() &&
			button->get_button_index() == MOUSE_BUTTON_LEFT) {
		dragging = button->is_pressed();
		drag_offset = button->get_position();
	}
	if (touch.is_valid()) {
		dragging = touch->is_pressed();
		drag_offset = touch->get_position();
	}
	if (dragging && motion.is_valid())
		hud->set_position(hud->get_position() + motion->get_relative());
	if (dragging && drag.is_valid())
		hud->set_position(hud->get_position() + drag->get_relative());
}
void DebugOverlay::refresh() {
	if (!center)
		return;
	show_modal(center->is_open());
	auto size = root->get_size(), pos = hud->get_position();
	hud->set_position(Vector2(
			std::clamp(pos.x, 0.f, std::max(0.f, size.x - hud->get_size().x)),
			std::clamp(pos.y, 0.f, std::max(0.f, size.y - hud->get_size().y))));
	auto ticks = Time::get_singleton()->get_ticks_msec();
	if (ticks < next_sample)
		return;
	next_sample = ticks + 250;
	double frames = Engine::get_singleton()->get_frames_per_second(),
		   ms = frames > 0 ? 1000 / frames : 0;
	fps->set_text(String::num(frames, 0) + " FPS | " + String::num(ms, 1) +
			" ms");
	center->samples.emplace_back(static_cast<real_t>(frames),
			static_cast<real_t>(ms));
	if (center->samples.size() > 240)
		center->samples.pop_front();
	if (!expanded)
		return;
	auto *performance = Performance::get_singleton();
	auto memory = performance->get_monitor(Performance::MEMORY_STATIC);
	auto *scene = get_tree()->get_current_scene();
	const char *net_names[] = { "Unknown", "No interface", "Local only",
		"Checking", "Online", "Unreachable" };
	overview->set_text(
			fps->get_text() + "\nUptime: " + String::num(ticks / 1000., 1) +
			" s | Memory: " +
			(memory > 0 ? String::num(memory / 1048576., 1) + " MiB"
						: String("N/A")) +
			" | Nodes: " +
			String::num(
					performance->get_monitor(Performance::OBJECT_NODE_COUNT), 0) +
			"\nScene: " +
			(scene ? scene->get_scene_file_path() : String("None")) +
			"\nLocal: " + App::time->format() + "\nUTC: " +
			App::time->format(TimeZone::UTC) + " | Network clock: " +
			(App::time->network_synced() ? String("synced")
										 : String("not synced")) +
			" | Network status: " +
			String(net_names[int(App::net->snapshot().status)]));
	graph->values = center->samples;
	graph->queue_redraw();
	log_panel->refresh();
	stats_text->set_text(center->stats());
	String text;
	for (const auto &line : center->output)
		text += line + String("\n");
	if (console->get_text() != text) {
		console->set_text(text);
		console->set_v_scroll(console->get_line_count());
	}
	if (selected_node && !ObjectDB::get_instance(ObjectID(selected_node))) {
		node_details->set_text("Selected node was freed");
		selected_node = 0;
	}
	if (panel_revision != center->panel_revision) {
		// Copy factories: they may register/unregister panels while
		// constructing.
		auto snapshot = center->panels;
		panel_revision = center->panel_revision;
		for (auto it = custom_panels.begin(); it != custom_panels.end();) {
			if (!snapshot.contains(it->first)) {
				if (auto *n = Object::cast_to<Node>(
							ObjectDB::get_instance(ObjectID(it->second)))) {
					tabs->remove_child(n);
					n->queue_free();
				}
				it = custom_panels.erase(it);
			} else
				++it;
		}
		for (const auto &[id, p] : snapshot)
			if (!custom_panels.contains(id)) {
				auto *node = p->factory();
				if (node && !node->get_parent()) {
					node->set_name(p->title);
					tabs->add_child(node);
					custom_panels[id] = uint64_t(node->get_instance_id());
				}
			}
	}
}
} // namespace meowody
