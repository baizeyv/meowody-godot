#pragma once
#include <deque>
#include <godot_cpp/classes/control.hpp>
#include <vector>

#include "log/log_record.h"

namespace meowody
{
	struct DebugCommandContext
	{
		godot::String			 line;
		godot::PackedStringArray args;
	};
	struct DebugCommandResult
	{
		bool					  ok = true;
		godot::String			  text;
		static DebugCommandResult success(godot::String text = {})
		{
			return {true, text};
		}
		static DebugCommandResult failure(godot::String text)
		{
			return {false, text};
		}
	};
	struct DebugCommand
	{
		godot::String name, description, usage;
		Delegate<DebugCommandResult(const DebugCommandContext&)>	   execute;
		Delegate<godot::PackedStringArray(const DebugCommandContext&)> complete;
	};
	struct DebugLogFilter
	{
		int			  level_mask = 31;
		godot::String tag, text;
		bool		  matches(const LogRecord& r) const
		{
			return (level_mask & (1 << r.level)) &&
				   (tag.is_empty() || r.tag.nocasecmp_to(tag) == 0) &&
				   (text.is_empty() || r.text.findn(text) >= 0 ||
					r.source.findn(text) >= 0);
		}
	};
	class DebugLogBuffer
	{
		std::deque<LogRecord> entries;
		size_t				  bytes = 0;

	  public:
		size_t	 max_records = 2000, max_bytes = 4 * 1024 * 1024,
				 max_message = 16 * 1024;
		uint64_t revision = 0, dropped = 0;
		void	 push(LogRecord record);
		void	 clear()
		{
			entries.clear();
			bytes	= 0;
			dropped = 0;
			++revision;
		}
		const std::deque<LogRecord>& records() const { return entries; }
	};
	DebugCommandResult parse_debug_command(const godot::String&,
										   godot::PackedStringArray&);
	godot::Variant	   debug_typed_value(const godot::Variant&, int depth = 0);
	godot::String debug_export_text(const godot::String& text,
									const godot::String& extension = "txt");
}  // namespace meowody
