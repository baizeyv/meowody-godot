#pragma once
#include <godot_cpp/variant/color.hpp>
#include <vector>

#include "log/log_record.h"
namespace meowody
{
	struct DebugLogSpan
	{
		godot::String text;
		godot::Color  color;
	};
	std::vector<DebugLogSpan> debug_log_spans(const LogRecord& record);
}  // namespace meowody
