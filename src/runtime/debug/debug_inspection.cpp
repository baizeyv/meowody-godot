#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/window.hpp>

#include "debug_overlay.h"
#include "runtime/app.h"

namespace meowody
{
	using namespace godot;
	static void value_tree(Tree* tree, TreeItem* parent, const String& name,
						   const Variant& value, int& budget, int depth = 0)
	{
		if (--budget < 0) return;
		auto* item = tree->create_item(parent);
		item->set_text(0, name);
		item->set_text(1, Variant::get_type_name(value.get_type()));
		if (depth > 16)
		{
			item->set_text(2, "[depth limit]");
			return;
		}
		if (value.get_type() == Variant::DICTIONARY)
		{
			Dictionary d	= value;
			auto	   keys = d.keys();
			item->set_text(2, String::num_int64(keys.size()) + " entries");
			for (int i = 0; i < keys.size() && budget > 0; ++i)
				value_tree(tree, item, String(keys[i]), d[keys[i]], budget,
						   depth + 1);
		}
		else if (value.get_type() == Variant::ARRAY)
		{
			Array a = value;
			item->set_text(2, String::num_int64(a.size()) + " entries");
			for (int i = 0; i < a.size() && budget > 0; ++i)
				value_tree(tree, item, String::num_int64(i), a[i], budget,
						   depth + 1);
		}
		else
			item->set_text(2, String(value).substr(0, 16384));
		if (depth > 0) item->set_collapsed(true);
	}
	void DebugOverlay::build_inspection_pages()
	{
		auto* saves = memnew(VBoxContainer);
		saves->set_name("Saves");
		tabs->add_child(saves);
		auto* actions = memnew(HBoxContainer);
		saves->add_child(actions);
		auto button = [&](Node* parent, const char* title, auto method) {
			auto* b = memnew(Button);
			b->set_text(title);
			parent->add_child(b);
			b->connect("pressed", callable_mp(this, method));
		};
		button(actions, "Refresh", &DebugOverlay::inspect_saves);
		button(actions, "Export raw disk file", &DebugOverlay::export_raw);
		button(actions, "Export plaintext JSON...",
			   &DebugOverlay::request_json);
		disk_mode = memnew(OptionButton);
		disk_mode->add_item("Latest (includes pending)");
		disk_mode->add_item("On disk");
		actions->add_child(disk_mode);
		save_status = memnew(Label);
		save_status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		saves->add_child(save_status);
		save_list = memnew(Tree);
		save_list->set_hide_root(true);
		save_list->set_custom_minimum_size(Vector2(0, 120));
		saves->add_child(save_list);
		save_list->connect("item_selected",
						   callable_mp(this, &DebugOverlay::select_save));
		save_data = memnew(Tree);
		save_data->set_columns(3);
		save_data->set_column_titles_visible(true);
		save_data->set_column_title(0, "Key");
		save_data->set_column_title(1, "Type");
		save_data->set_column_title(2, "Value");
		save_data->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		saves->add_child(save_data);
		plaintext = memnew(ConfirmationDialog);
		plaintext->set_title("Export decrypted data?");
		plaintext->set_text(
			"This JSON is plaintext and may expose private saved data. It is a "
			"diagnostic snapshot, not an importable save.");
		root->add_child(plaintext);
		plaintext->connect("confirmed",
						   callable_mp(this, &DebugOverlay::export_json));
		stats_text = memnew(TextEdit);
		stats_text->set_name("Runtime");
		stats_text->set_editable(false);
		tabs->add_child(stats_text);
		auto* scene = memnew(VBoxContainer);
		scene->set_name("Scene Tree");
		tabs->add_child(scene);
		auto* search_row = memnew(HBoxContainer);
		scene->add_child(search_row);
		node_search = memnew(LineEdit);
		node_search->set_placeholder("Node name / path");
		node_search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		search_row->add_child(node_search);
		button(search_row, "Search / Refresh", &DebugOverlay::refresh_nodes);
		nodes = memnew(Tree);
		nodes->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		scene->add_child(nodes);
		nodes->connect("item_selected",
					   callable_mp(this, &DebugOverlay::select_node));
		nodes->connect("item_collapsed",
					   callable_mp(this, &DebugOverlay::expand_node));
		node_details = memnew(TextEdit);
		node_details->set_editable(false);
		node_details->set_custom_minimum_size(Vector2(0, 150));
		scene->add_child(node_details);
		auto* tools = memnew(VBoxContainer);
		tools->set_name("Tools");
		tabs->add_child(tools);
		auto* note = memnew(Label);
		note->set_text(
			"SceneTree pause does NOT pause Runtime services or coroutines.");
		tools->add_child(note);
		auto* row = memnew(HBoxContainer);
		tools->add_child(row);
		button(row, "Pause SceneTree", &DebugOverlay::pause_tree);
		button(row, "Resume", &DebugOverlay::resume_tree);
		button(row, "Reset time scale", &DebugOverlay::reset_scale);
		auto* scale = memnew(SpinBox);
		scale->set_min(0);
		scale->set_max(10);
		scale->set_step(.1);
		scale->set_value(Engine::get_singleton()->get_time_scale());
		scale->set_prefix("Time scale");
		row->add_child(scale);
		scale->connect("value_changed",
					   callable_mp(this, &DebugOverlay::scale_changed));
		button(row, "Refresh audio", &DebugOverlay::refresh_audio);
		auto* scroll = memnew(ScrollContainer);
		scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		tools->add_child(scroll);
		audio_rows = memnew(VBoxContainer);
		audio_rows->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		scroll->add_child(audio_rows);
		refresh_audio();
		inspect_saves();
		refresh_nodes();
	}
	void DebugOverlay::refresh_audio()
	{
		if (!audio_rows) return;
		while (audio_rows->get_child_count())
		{
			auto* child = audio_rows->get_child(0);
			audio_rows->remove_child(child);
			child->queue_free();
		}
		auto channels = App::audio->channels();
		for (int i = 0; i < channels.size(); ++i)
		{
			auto  opts	  = App::audio->channel_options(channels[i]);
			auto* channel = memnew(HBoxContainer);
			audio_rows->add_child(channel);
			auto* label = memnew(Label);
			label->set_text(channels[i]);
			channel->add_child(label);
			auto* volume = memnew(SpinBox);
			volume->set_min(0);
			volume->set_max(1);
			volume->set_step(.05);
			volume->set_value(opts.volume);
			channel->add_child(volume);
			volume->connect("value_changed",
							callable_mp(this, &DebugOverlay::audio_volume)
								.bind(channels[i]));
			auto* mute = memnew(CheckBox);
			mute->set_text("Mute");
			mute->set_pressed(opts.muted);
			channel->add_child(mute);
			mute->connect(
				"toggled",
				callable_mp(this, &DebugOverlay::audio_mute).bind(channels[i]));
		}
	}

	void DebugOverlay::inspect_saves()
	{
		if (!center) return;
		save_list->clear();
		save_data->clear();
		selected_part	= String();
		selected_slot	= String();
		auto* root_item = save_list->create_item();
		auto& saves		= App::save;
		auto  parts		= saves->list_parts();
		for (int i = 0; i < parts.size(); ++i)
		{
			auto* part = save_list->create_item(root_item);
			part->set_text(
				0, parts[i] + (saves->is_pending(parts[i]) ? String(" [pending]")
														  : String()));
			Array data;
			data.push_back(parts[i]);
			data.push_back("");
			part->set_metadata(0, data);
			auto r = saves->inspect(parts[i], disk_mode->get_selected() == 1);
			if (!r.ok())
			{
				part->set_tooltip_text(0, r.message);
				continue;
			}
			auto slots = r.data.keys();
			for (int j = 0; j < slots.size(); ++j)
			{
				auto* slot = save_list->create_item(part);
				slot->set_text(0, slots[j]);
				Array meta;
				meta.push_back(parts[i]);
				meta.push_back(slots[j]);
				slot->set_metadata(0, meta);
			}
		}
		save_status->set_text(
			saves->directory() +
			" | Pending: " + String::num_uint64(saves->pending_count()) +
			" | Commits: " + String::num_uint64(saves->commit_count()) +
			" | Select a partition/slot. Refresh after changing source.");
	}
	void DebugOverlay::select_save()
	{
		auto* item = save_list->get_selected();
		if (!item) return;
		Array meta = item->get_metadata(0);
		if (meta.size() != 2) return;
		selected_part = meta[0];
		selected_slot = meta[1];
		auto r = App::save->inspect(selected_part,
									  disk_mode->get_selected() == 1);
		save_data->clear();
		save_status->set_text(r.ok() ? String("Read-only: ") + selected_part +
										   "/" + selected_slot
									 : r.message);
		if (!r.ok()) return;
		Variant value = r.data;
		if (!selected_slot.is_empty())
		{
			if (!r.data.has(selected_slot))
			{
				save_status->set_text("Slot unavailable in selected source");
				return;
			}
			value = r.data[selected_slot];
		}
		int budget = 2000;
		value_tree(save_data, nullptr, "Data", value, budget);
		if (budget <= 0) save_status->set_text("Tree limited to 2000 entries");
	}
	void DebugOverlay::export_raw()
	{
		if (center && !selected_part.is_empty())
			save_status->set_text(
				center->export_save(selected_part, {}, true, true));
	}
	void DebugOverlay::request_json()
	{
		if (!selected_part.is_empty()) plaintext->popup_centered();
	}
	void DebugOverlay::export_json()
	{
		if (center && !selected_part.is_empty())
			save_status->set_text(
				center->export_save(selected_part, selected_slot,
									disk_mode->get_selected() == 1, false));
	}
	void DebugOverlay::populate_node(Node* node, TreeItem* parent, int& budget,
									 bool recurse)
	{
		if (!node || --budget < 0) return;
		bool matches =
			node_search->get_text().is_empty() ||
			String(node->get_path()).findn(node_search->get_text()) >= 0;
		TreeItem* item = parent;
		if (matches)
		{
			item = nodes->create_item(parent);
			item->set_text(
				0, String(node->get_name()) + " : " + node->get_class());
			item->set_metadata(0, int64_t(node->get_instance_id()));
		}
		if (recurse)
		{
			for (int i = 0; i < node->get_child_count() && budget > 0; ++i)
				populate_node(node->get_child(i), item, budget, true);
		}
		else if (matches && node->get_child_count())
		{
			nodes->create_item(item)->set_text(0, "Expand to inspect...");
			item->set_collapsed(true);
		}
	}
	void DebugOverlay::refresh_nodes()
	{
		pending_expansions.clear();
		nodes->clear();
		selected_node = 0;
		node_details->clear();
		int budget = 2000;
		populate_node(get_tree()->get_root(), nullptr, budget,
					  !node_search->get_text().is_empty());
	}
	void DebugOverlay::expand_node(TreeItem* item)
	{
		if (!item || item->is_collapsed()) return;
		pending_expansions.push_back(uint64_t(item->get_instance_id()));
		if (!expansion_scheduled)
		{
			expansion_scheduled = true;
			call_deferred("flush_node_expansions");
		}
	}
	void DebugOverlay::flush_node_expansions()
	{
		expansion_scheduled = false;
		auto pending		= std::move(pending_expansions);
		pending_expansions.clear();
		if (!center || !nodes) return;
		for (auto id : pending)
		{
			auto* item =
				Object::cast_to<TreeItem>(ObjectDB::get_instance(ObjectID(id)));
			if (!item || item->get_tree() != nodes || item->is_collapsed())
				continue;
			auto* node = Object::cast_to<Node>(ObjectDB::get_instance(
				ObjectID(uint64_t(int64_t(item->get_metadata(0))))));
			if (!node) continue;
			auto* child = item->get_first_child();
			if (!child || child->get_text(0) != "Expand to inspect...")
				continue;
			item->remove_child(child);
			memdelete(child);
			int budget = 2000;
			for (int i = 0; i < node->get_child_count() && budget > 0; ++i)
				populate_node(node->get_child(i), item, budget, false);
		}
	}
	void DebugOverlay::select_node()
	{
		auto* item = nodes->get_selected();
		if (!item) return;
		selected_node = uint64_t(int64_t(item->get_metadata(0)));
		auto* node	  = Object::cast_to<Node>(
			ObjectDB::get_instance(ObjectID(selected_node)));
		if (!node)
		{
			node_details->set_text("Node unavailable");
			return;
		}
		String text =
			String(node->get_path()) + "\n" + node->get_class() +
			" | Process mode: " + String::num_int64(node->get_process_mode()) +
			"\n";
		auto props = node->get_property_list();
		for (int i = 0; i < props.size() && i < 2000; ++i)
		{
			Dictionary p = props[i];
			if (int(p["usage"]) & PROPERTY_USAGE_EDITOR)
			{
				String name = p["name"];
				if (int(p["usage"]) &
					(PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP |
					 PROPERTY_USAGE_SUBGROUP))
					continue;
				text += name + " = " + String(node->get(name)).substr(0, 1024) +
						"\n";
			}
		}
		node_details->set_text(text);
	}
	void DebugOverlay::pause_tree()
	{
		if (center) center->execute_command("pause");
	}
	void DebugOverlay::resume_tree()
	{
		if (center) center->execute_command("resume");
	}
	void DebugOverlay::reset_scale()
	{
		if (center) center->execute_command("timescale 1");
	}
	void DebugOverlay::scale_changed(double v)
	{
		if (center) center->execute_command("timescale " + String::num(v));
	}
	void DebugOverlay::audio_volume(double v, String channel)
	{
		App::audio->set_volume(channel, v);
	}
	void DebugOverlay::audio_mute(bool v, String channel)
	{
		App::audio->set_muted(channel, v);
	}
}  // namespace meowody
