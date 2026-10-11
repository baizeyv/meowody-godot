#pragma once
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

#include "debug_log_style.h"
#include "debug_types.h"
#include "reflection/reflection.h"

namespace meowody
{
	class DebugLogPanel : public godot::VBoxContainer
	{
		GDREFLECT_CLASS(DebugLogPanel, godot::VBoxContainer)
		DebugLogBuffer*		  buffer  = nullptr;
		godot::Tree*		  list	  = nullptr;
		godot::RichTextLabel* details = nullptr;
		godot::LineEdit *	  search = nullptr, *tag = nullptr;
		godot::CheckBox *	  collapse = nullptr, *paused = nullptr,
						*follow = nullptr;
		godot::CheckBox*					levels[5]{};
		godot::Label*						count	 = nullptr;
		uint64_t							revision = ~uint64_t(0);
		godot::String						filter_key;
		std::vector<std::vector<LogRecord>> rows;

	  public:
		std::vector<std::vector<DebugLogSpan>> styled_rows;
		void draw_message(godot::TreeItem*, godot::Rect2);
		void setup(DebugLogBuffer*);
		void refresh();
		void selected();
		void clear_records();
		void copy();
		void export_records();
	};
}  // namespace meowody
