#pragma once
#include <map>
#include <set>

#include "debug_types.h"

namespace meowody
{
	class DebugOverlay;
	class DebugCenter
	{
		bool	 enabled = false, opened = false, stopped = false, busy = false;
		uint64_t overlay_id = 0;
		Connection											   log_connection;
		DebugLogBuffer										   incoming;
		std::map<godot::String, std::shared_ptr<DebugCommand>> commands;
		std::set<godot::String>								   builtins;
		std::deque<godot::String>							   queue;
		void install_commands();
		void ensure_overlay();

	  public:
		struct Panel
		{
			godot::String				title;
			Delegate<godot::Control*()> factory;
		};
		std::map<godot::String, std::shared_ptr<Panel>> panels;
		uint64_t										panel_revision = 0;
		DebugLogBuffer									logs;
		std::deque<godot::String>						history, output;
		std::deque<godot::Vector2>						samples;
		godot::Key toggle_key = godot::KEY_F12;
		DebugCenter();
		~DebugCenter();
		void set_enabled(bool);
		bool is_enabled() const { return enabled; }
		bool is_open() const { return enabled && opened; }
		bool captures_input() const;
		void open();
		void close();
		void toggle()
		{
			if (is_open())
				close();
			else
				open();
		}
		bool register_command(DebugCommand&&);
		bool register_command(DebugCommand& command)
		{
			return register_command(std::move(command));
		}
		bool					 unregister_command(const godot::String&);
		bool					 execute_command(const godot::String&);
		godot::PackedStringArray complete(const godot::String&) const;
		void		  print(const godot::String&, bool error = false);
		bool		  register_panel(const godot::String&, const godot::String&,
									 Delegate<godot::Control*()>);
		bool		  unregister_panel(const godot::String&);
		godot::String stats() const;
		godot::String export_save(const godot::String& part,
								  const godot::String& slot, bool disk,
								  bool raw);
		void		  update(double);
		void		  shutdown();
	};
}  // namespace meowody
