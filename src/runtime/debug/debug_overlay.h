#pragma once
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/confirmation_dialog.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/text_edit.hpp>

#include "debug_center.h"
#include "debug_log_panel.h"

namespace meowody
{
	class DebugGraph : public godot::Control
	{
		GDREFLECT_CLASS(DebugGraph, godot::Control)
	  public:
		std::deque<godot::Vector2> values;
		void					   _draw() override;
	};
	class DebugOverlay : public godot::CanvasLayer
	{
		GDREFLECT_CLASS(DebugOverlay, godot::CanvasLayer)
		DebugCenter*		   center	  = nullptr;
		godot::VBoxContainer*  audio_rows = nullptr;
		godot::Control*		   root		  = nullptr;
		godot::PanelContainer *hud = nullptr, *modal = nullptr;
		godot::Label *		   fps = nullptr, *overview = nullptr,
					 *save_status = nullptr;
		godot::TextEdit *console = nullptr, *stats_text = nullptr,
						*node_details = nullptr;
		godot::LineEdit *	 command = nullptr, *node_search = nullptr;
		godot::TabContainer* tabs	   = nullptr;
		DebugLogPanel*		 log_panel = nullptr;
		DebugGraph*			 graph	   = nullptr;
		godot::Tree *		 save_list = nullptr, *save_data = nullptr,
					*nodes					 = nullptr;
		godot::OptionButton*	   disk_mode = nullptr;
		godot::ConfirmationDialog* plaintext = nullptr;
		godot::String			   selected_part, selected_slot;
		uint64_t	   focus_id = 0, selected_node = 0, next_sample = 0,
					   panel_revision = ~uint64_t(0);
		bool		   expanded = false, dragging = false;
		int			   history_index = -1;
		godot::Vector2 drag_offset;
		std::map<godot::String, uint64_t> custom_panels;
		std::vector<uint64_t>			  pending_expansions;
		bool							  expansion_scheduled = false;
		void							  build_inspection_pages();
		void populate_node(godot::Node*, godot::TreeItem*, int&, bool recurse);

	  public:
		void setup(DebugCenter*);
		void detach()
		{
			center = nullptr;
			set_process_input(false);
		}
		void		  refresh();
		void		  show_modal(bool);
		void		  _input(const godot::Ref<godot::InputEvent>&) override;
		void		  hud_input(const godot::Ref<godot::InputEvent>&);
		void		  submit(const godot::String&);
		void		  toggle();
		void		  inspect_saves();
		void		  select_save();
		void		  export_raw();
		void		  request_json();
		void		  export_json();
		void		  refresh_nodes();
		void		  select_node();
		void		  expand_node(godot::TreeItem*);
		GDMETHOD void flush_node_expansions();
		void		  pause_tree();
		void		  resume_tree();
		void		  reset_scale();
		void		  scale_changed(double);
		void		  audio_volume(double, godot::String);
		void		  audio_mute(bool, godot::String);
		void		  refresh_audio();
	};
}  // namespace meowody
