#include "debug_log_panel.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <map>

namespace meowody
{
	using namespace godot;
	static String detail_text(const LogRecord& r)
	{
		String text = "#" + String::num_uint64(r.sequence) + "  " +
					  String::num(r.timestamp, 3) + "  " + r.source + "  [" +
					  r.tag + "]\n" + r.text + "\n\nNative stack:\n";
		if (r.stack.is_empty()) text += "Not captured\n";
		for (int i = 0; i < r.stack.size(); ++i)
			text += r.stack[i] + String("\n");
		return text;
	}
	void DebugLogPanel::setup(DebugLogBuffer* source)
	{
		buffer	  = source;
		auto* bar = memnew(HBoxContainer);
		add_child(bar);
		const char* names[] = {"Debug", "Info", "Warn", "Error", "Fatal"};
		for (int i = 0; i < 5; ++i)
		{
			levels[i] = memnew(CheckBox);
			levels[i]->set_text(names[i]);
			levels[i]->set_pressed(true);
			bar->add_child(levels[i]);
		}
		auto* filters = memnew(HBoxContainer);
		add_child(filters);
		search = memnew(LineEdit);
		search->set_placeholder("Search text / source");
		search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		filters->add_child(search);
		tag = memnew(LineEdit);
		tag->set_placeholder("Exact Tag");
		tag->set_custom_minimum_size(Vector2(120, 0));
		filters->add_child(tag);
		collapse = memnew(CheckBox);
		collapse->set_text("Collapse");
		filters->add_child(collapse);
		paused = memnew(CheckBox);
		paused->set_text("Pause display");
		filters->add_child(paused);
		follow = memnew(CheckBox);
		follow->set_text("Auto scroll");
		follow->set_pressed(true);
		filters->add_child(follow);
		auto button = [&](const char* name, auto method) {
			auto* b = memnew(Button);
			b->set_text(name);
			bar->add_child(b);
			b->connect("pressed", callable_mp(this, method));
		};
		button("Clear", &DebugLogPanel::clear_records);
		button("Copy detail", &DebugLogPanel::copy);
		button("Export filtered", &DebugLogPanel::export_records);
		count = memnew(Label);
		add_child(count);
		list = memnew(Tree);
		list->set_columns(4);
		list->set_column_titles_visible(true);
		list->set_column_title(0, "Level / Count");
		list->set_column_title(1, "Time / Source");
		list->set_column_title(2, "Tag");
		list->set_column_title(3, "Message");
		list->set_hide_root(true);
		list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		list->set_custom_minimum_size(Vector2(0, 140));
		add_child(list);
		list->connect("item_selected",
					  callable_mp(this, &DebugLogPanel::selected));
		for (int i = 0; i < 3; ++i)
		{
			list->set_column_expand(i, false);
			list->set_column_custom_minimum_width(i, i == 1 ? 180 : 100);
		}
		details = memnew(RichTextLabel);
		details->set_selection_enabled(true);
		details->set_use_bbcode(false);
		details->add_theme_color_override("font_readonly_color",
										  Color(.85, .87, .9));
		details->set_custom_minimum_size(Vector2(0, 160));
		details->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		add_child(details);
		refresh();
	}
	void DebugLogPanel::refresh()
	{
		if (!buffer || paused->is_pressed()) return;
		DebugLogFilter f;
		f.level_mask = 0;
		for (int i = 0; i < 5; ++i)
			if (levels[i]->is_pressed()) f.level_mask |= 1 << i;
		f.tag	   = tag->get_text();
		f.text	   = search->get_text();
		String key = String::num_int64(f.level_mask) + "|" + f.tag + "|" +
					 f.text + "|" + String::num_int64(collapse->is_pressed());
		if (revision == buffer->revision && key == filter_key) return;
		revision			 = buffer->revision;
		filter_key			 = key;
		uint64_t selected_id = 0;
		if (auto* item = list->get_selected())
		{
			auto index = int(item->get_metadata(0));
			if (index >= 0 && size_t(index) < rows.size())
				selected_id = rows[index].back().sequence;
		}
		rows.clear();
		styled_rows.clear();
		list->clear();
		auto*					 root = list->create_item();
		std::map<String, size_t> groups;
		for (const auto& r : buffer->records())
		{
			if (!f.matches(r)) continue;
			String group = String::num_int64(r.level) + "\x1f" + r.tag +
						   "\x1f" + r.source + "\x1f" + r.text +
						   (r.rich ? "rich" : "plain");
			if (collapse->is_pressed() && groups.contains(group))
				rows[groups[group]].push_back(r);
			else
			{
				groups[group] = rows.size();
				rows.push_back({r});
			}
		}
		const char* names[] = {"Debug", "Info", "Warn", "Error", "Fatal"};
		TreeItem*	last	= nullptr;
		for (size_t i = 0; i < rows.size(); ++i)
		{
			const auto& r	 = rows[i].back();
			auto*		item = list->create_item(root);
			last			 = item;
			item->set_metadata(0, int64_t(i));
			item->set_text(0, String(names[r.level]) + " x" +
								  String::num_uint64(rows[i].size()));
			item->set_text(1, String::num(r.timestamp, 3) + " " + r.source);
			item->set_text(2, r.tag);
			styled_rows.push_back(debug_log_spans(r));
			String plain;
			for (const auto& span : styled_rows.back()) plain += span.text;
			item->set_cell_mode(3, TreeItem::CELL_MODE_CUSTOM);
			item->set_metadata(3, plain.get_slice("\n", 0));
			item->set_tooltip_text(3, plain);
			item->set_custom_draw_callback(
				3, callable_mp(this, &DebugLogPanel::draw_message));
			for (const auto& occurrence : rows[i])
				if (occurrence.sequence == selected_id) item->select(0);
		}
		count->set_text(String::num_uint64(rows.size()) + " rows / " +
						String::num_uint64(buffer->records().size()) +
						" retained / " + String::num_uint64(buffer->dropped) +
						" dropped");
		if (last && follow->is_pressed()) list->scroll_to_item(last);
	}
	void DebugLogPanel::draw_message(TreeItem* item, Rect2 rect)
	{
		auto index = int64_t(item->get_metadata(0));
		if (index < 0 || size_t(index) >= styled_rows.size()) return;
		auto font = list->get_theme_font("font");
		auto size = list->get_theme_font_size("font_size");
		// Tree invokes custom draw for partly clipped rows as well; do not
		// paint over titles.
		const auto header = font->get_height(size) +
							list->get_theme_stylebox("title_button_normal")
								->get_minimum_size()
								.y;
		if (rect.position.y < header ||
			rect.position.y + rect.size.y > list->get_size().y)
			return;
		auto   pos = rect.position +
					 Vector2(0, (rect.size.y - font->get_height(size)) * .5 +
									font->get_ascent(size));
		double remaining = rect.size.x;
		for (const auto& span : styled_rows[index])
		{
			if (remaining <= 0) break;
			auto text = span.text.get_slice("\n", 0);
			list->draw_string(font, pos, text, HORIZONTAL_ALIGNMENT_LEFT,
							  remaining, size, span.color);
			auto width =
				font->get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, size)
					.x;
			pos.x += width;
			remaining -= width;
			if (span.text.contains("\n")) break;
		}
	}
	void DebugLogPanel::selected()
	{
		auto* item = list->get_selected();
		if (!item) return;
		auto index = int(item->get_metadata(0));
		if (index < 0 || size_t(index) >= rows.size()) return;
		details->clear();
		int64_t length = 0;
		for (const auto& r : rows[index])
		{
			if (length > 512 * 1024)
			{
				details->add_text("\n[detail limit]");
				break;
			}
			details->add_text("#" + String::num_uint64(r.sequence) + "  " +
							  r.source + " [" + r.tag + "]\n");
			for (const auto& span : debug_log_spans(r))
			{
				details->push_color(span.color);
				details->add_text(span.text);
				details->pop();
			}
			details->add_text("\n\nCall stack:\n");
			if (r.stack.is_empty()) details->add_text("Not captured\n");
			for (int i = 0; i < r.stack.size(); ++i)
				details->add_text(r.stack[i] + String("\n"));
			details->add_text("\n---\n");
			length += detail_text(r).length();
		}
	}
	void DebugLogPanel::clear_records()
	{
		if (buffer) buffer->clear();
		details->clear();
		refresh();
	}
	void DebugLogPanel::copy()
	{
		DisplayServer::get_singleton()->clipboard_set(
			details->get_parsed_text());
	}
	void DebugLogPanel::export_records()
	{
		String text;
		for (const auto& row : rows)
			for (const auto& r : row) text += detail_text(r) + "\n";
		auto path = debug_export_text(text);
		details->set_text(path.is_empty() ? String("Export failed")
										  : String("Exported: ") + path);
	}
}  // namespace meowody
