#pragma once
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <source_location>

#include "runtime/core/connection.hpp"
#include "runtime/core/delegate.hpp"

namespace meowody
{
	struct LogRecord
	{
		uint64_t				 sequence  = 0;
		double					 timestamp = 0;
		bool					 rich	   = false;
		int						 level	   = 0;
		godot::String			 tag, text, source = "Local";
		godot::PackedStringArray stack;
	};
	namespace detail
	{
		void publish_log(int level, const godot::String& tag,
						 const godot::String& text,
						 std::source_location location);
		void shutdown_log_records();
		godot::PackedStringArray capture_log_stack();
	}  // namespace detail
}  // namespace meowody
