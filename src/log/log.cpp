//
// Created by baizeyv on 8/30/2026.
//

#include "log.hpp"

#include <godot_cpp/variant/utility_functions.hpp>
#include <unordered_map>
#include <unordered_set>

namespace meowody
{
	// ============================================================================
	// Internal tag configuration
	// ============================================================================

	namespace
	{
		struct StringNameHash
		{
			size_t operator()(const StringName& value) const noexcept
			{
				return static_cast<size_t>(value.hash());
			}
		};

		struct StringNameEqual
		{
			bool operator()(const StringName& a,
							const StringName& b) const noexcept
			{
				return a == b;
			}
		};

		using TagLevelMap = std::unordered_map<StringName, Log::Level,
											   StringNameHash, StringNameEqual>;

		using TagEnabledSet =
			std::unordered_set<StringName, StringNameHash, StringNameEqual>;

		// Tag 独立 Level
		TagLevelMap tag_levels;

		// 被关闭的 Tag
		TagEnabledSet disabled_tags;
	}  // namespace

	// ============================================================================
	// Static members
	// ============================================================================

	Log::Level Log::_level = Log::Level::Debug;

	bool Log::_rich_enabled = true;

	// Godot constructors are unavailable during DLL static initialization.
	std::optional<StringName> Log::_default_tag;

	// ============================================================================
	// Global configuration
	// ============================================================================

	void Log::set_level(Level level) { _level = level; }

	Log::Level Log::get_level() { return _level; }

	bool Log::is_enabled(Level level)
	{
		return static_cast<uint8_t>(level) >= static_cast<uint8_t>(_level);
	}

	void Log::set_rich_enabled(bool enabled) { _rich_enabled = enabled; }

	bool Log::is_rich_enabled() { return _rich_enabled; }

	void Log::set_default_tag(const StringName& tag) { _default_tag = tag; }

	const StringName& Log::get_default_tag()
	{
		if (!_default_tag) _default_tag.emplace("Game");
		return *_default_tag;
	}

	void Log::shutdown()
	{
		detail::shutdown_log_records();
		clear_tag_settings();
		_default_tag.reset();
	}

	// ============================================================================
	// Tag configuration
	// ============================================================================

	void Log::set_tag_level(const StringName& tag, Level level)
	{
		if (tag.is_empty())
		{
			return;
		}

		tag_levels[tag] = level;
	}

	void Log::reset_tag_level(const StringName& tag)
	{
		if (tag.is_empty())
		{
			return;
		}

		tag_levels.erase(tag);
	}

	Log::Level Log::get_tag_level(const StringName& tag)
	{
		if (tag.is_empty())
		{
			return _level;
		}

		auto iterator = tag_levels.find(tag);

		if (iterator == tag_levels.end())
		{
			return _level;
		}

		return iterator->second;
	}

	void Log::set_tag_enabled(const StringName& tag, bool enabled)
	{
		if (tag.is_empty())
		{
			return;
		}

		if (enabled)
		{
			disabled_tags.erase(tag);
		}
		else
		{
			disabled_tags.insert(tag);
		}
	}

	bool Log::is_tag_enabled(const StringName& tag)
	{
		if (tag.is_empty())
		{
			return true;
		}

		return disabled_tags.find(tag) == disabled_tags.end();
	}

	void Log::clear_tag_settings()
	{
		tag_levels.clear();

		disabled_tags.clear();
	}

	// ============================================================================
	// Should Log
	// ============================================================================

	bool Log::should_log(Level level, const StringName& tag)
	{
		// ------------------------------------------------------------
		// Tag disabled
		// ------------------------------------------------------------

		if (!tag.is_empty())
		{
			if (disabled_tags.find(tag) != disabled_tags.end())
			{
				return false;
			}
		}

		// ------------------------------------------------------------
		// Find level
		// ------------------------------------------------------------

		Level minimum_level = _level;

		if (!tag.is_empty())
		{
			auto iterator = tag_levels.find(tag);

			if (iterator != tag_levels.end())
			{
				minimum_level = iterator->second;
			}
		}

		// ------------------------------------------------------------
		// Compare
		// ------------------------------------------------------------

		return static_cast<uint8_t>(level) >=
			   static_cast<uint8_t>(minimum_level);
	}

	// ============================================================================
	// Level Name
	// ============================================================================

	const char* Log::level_name(Level level)
	{
		switch (level)
		{
			case Level::Debug:
				return "DEBUG";

			case Level::Info:
				return "INFO";

			case Level::Warn:
				return "WARN";

			case Level::Error:
				return "ERROR";

			case Level::Fatal:
				return "FATAL";

			default:
				return "UNKNOWN";
		}
	}

	// ============================================================================
	// Level Color
	// ============================================================================

	const char* Log::level_color(Level level)
	{
		switch (level)
		{
			case Level::Debug:
				return "#808080";

			case Level::Info:
				return "#55AAFF";

			case Level::Warn:
				return "#FFCC55";

			case Level::Error:
				return "#FF5555";

			case Level::Fatal:
				return "#FF00FF";

			default:
				return "#FFFFFF";
		}
	}

	// ============================================================================
	// Make Message
	// ============================================================================

	String Log::make_message(Level level, const StringName& tag,
							 std::string_view message)
	{
		const StringName& actual_tag = tag.is_empty() ? get_default_tag() : tag;

		String tag_string(actual_tag);

		CharString tag_utf8 = tag_string.utf8();

		// ------------------------------------------------------------
		// Plain text
		// ------------------------------------------------------------

		if (!_rich_enabled)
		{
			std::ostringstream stream;

			stream << "[" << level_name(level) << "]"
				   << "[" << tag_utf8.get_data() << "] " << message;

			return String::utf8(stream.str().c_str());
		}

		// ------------------------------------------------------------
		// Rich text
		// ------------------------------------------------------------

		std::ostringstream stream;

		stream << "[color=" << level_color(level) << "]"

			   << "[" << level_name(level) << "]"

			   << "[" << tag_utf8.get_data() << "]"

			   << "[/color] "

			   << message;

		return String::utf8(stream.str().c_str());
	}

	// ============================================================================
	// Direct Print
	// ============================================================================

	void Log::print(Level level, const String& message, const StringName& tag, std::source_location location)
	{
		if (!should_log(level, tag))
		{
			return;
		}

		CharString utf8 = message.utf8();

		print(level, std::string_view(utf8.get_data(), utf8.length()), tag, location);
	}

	void Log::print(Level level, std::string_view message,
					const StringName& tag, std::source_location location)
	{
		if (!should_log(level, tag))
		{
			return;
		}

		detail::publish_log(int(level), tag == StringName() ? String(get_default_tag()) : String(tag), String::utf8(message.data(), int64_t(message.size())), location);
		String output = make_message(level, tag, message);

		if (_rich_enabled)
		{
			UtilityFunctions::print_rich(output);
		}
		else
		{
			UtilityFunctions::print(output);
		}
	}

	// ============================================================================
	// Stream Constructor
	// ============================================================================

	Log::Stream::Stream(Level level, const StringName& tag, std::source_location location) :
		_level(level), _location(location), _tag(tag), _enabled(Log::should_log(level, tag))
	{
	}

	Log::Stream::Stream(Level level, std::string_view tag, std::source_location location) :
		_level(level), _location(location),
		_tag(StringName(String::utf8(tag.data(), static_cast<int64_t>(tag.size())))),
		_enabled(Log::should_log(level, _tag))
	{
	}

	// ============================================================================
	// Stream Destructor
	// ============================================================================

	Log::Stream::~Stream() { flush(); }

	// ============================================================================
	// Move Constructor
	// ============================================================================

	Log::Stream::Stream(Stream&& other) noexcept :
		_level(other._level), _location(other._location),
		_tag(std::move(other._tag)),
		_stream(std::move(other._stream)),
		_enabled(other._enabled),
		_flushed(other._flushed)
	{
		other._flushed = true;
	}

	// ============================================================================
	// Move Assignment
	// ============================================================================

	Log::Stream& Log::Stream::operator=(Stream&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}

		flush();

		_level = other._level;
        _location = other._location;

		_tag = std::move(other._tag);

		_stream = std::move(other._stream);

		_enabled = other._enabled;

		_flushed = other._flushed;

		other._flushed = true;

		return *this;
	}

	// ============================================================================
	// Flush
	// ============================================================================

	void Log::Stream::flush()
	{
		if (!_enabled)
		{
			return;
		}

		if (_flushed)
		{
			return;
		}

		_flushed = true;

		Log::print(_level, _stream.str(), _tag, _location);
	}

	// ============================================================================
	// Manipulator
	// ============================================================================

	Log::Stream& Log::Stream::operator<<(Manipulator manipulator)
	{
		if (_enabled)
		{
			manipulator(_stream);
		}

		return *this;
	}

	// ============================================================================
	// String
	// ============================================================================

	Log::Stream& Log::Stream::operator<<(const String& value)
	{
		if (!_enabled)
		{
			return *this;
		}

		CharString utf8 = value.utf8();

		_stream << utf8.get_data();

		return *this;
	}

	// ============================================================================
	// StringName
	// ============================================================================

	Log::Stream& Log::Stream::operator<<(const StringName& value)
	{
		if (!_enabled)
		{
			return *this;
		}

		String string_value(value);

		CharString utf8 = string_value.utf8();

		_stream << utf8.get_data();

		return *this;
	}

	// ============================================================================
	// Variant
	// ============================================================================

	Log::Stream& Log::Stream::operator<<(const Variant& value)
	{
		if (!_enabled)
		{
			return *this;
		}

		String string_value = value.stringify();

		CharString utf8 = string_value.utf8();

		_stream << utf8.get_data();

		return *this;
	}

	// ============================================================================
	// Object
	// ============================================================================

	Log::Stream& Log::Stream::operator<<(const Object* object)
	{
		if (!_enabled)
		{
			return *this;
		}

		if (object == nullptr)
		{
			_stream << "nullptr";

			return *this;
		}

		_stream << "<" << object->get_class().utf8().get_data() << ":"
				<< object->get_instance_id() << ">";

		return *this;
	}

	// ============================================================================
	// Debug
	// ============================================================================

	Log::Stream Log::debug(std::source_location location) { return Stream(Level::Debug, StringName(), location); }

	Log::Stream Log::debug(const StringName& tag, std::source_location location)
	{
		return Stream(Level::Debug, tag, location);
	}

	Log::Stream Log::debug(std::string_view tag, std::source_location location)
	{
		return Stream(Level::Debug, tag, location);
	}

	// ============================================================================
	// Info
	// ============================================================================

	Log::Stream Log::info(std::source_location location) { return Stream(Level::Info, StringName(), location); }

	Log::Stream Log::info(const StringName& tag, std::source_location location)
	{
		return Stream(Level::Info, tag, location);
	}

	Log::Stream Log::info(std::string_view tag, std::source_location location)
	{
		return Stream(Level::Info, tag, location);
	}

	// ============================================================================
	// Warn
	// ============================================================================

	Log::Stream Log::warn(std::source_location location) { return Stream(Level::Warn, StringName(), location); }

	Log::Stream Log::warn(const StringName& tag, std::source_location location)
	{
		return Stream(Level::Warn, tag, location);
	}

	Log::Stream Log::warn(std::string_view tag, std::source_location location)
	{
		return Stream(Level::Warn, tag, location);
	}

	// ============================================================================
	// Error
	// ============================================================================

	Log::Stream Log::error(std::source_location location) { return Stream(Level::Error, StringName(), location); }

	Log::Stream Log::error(const StringName& tag, std::source_location location)
	{
		return Stream(Level::Error, tag, location);
	}

	Log::Stream Log::error(std::string_view tag, std::source_location location)
	{
		return Stream(Level::Error, tag, location);
	}

	// ============================================================================
	// Fatal
	// ============================================================================

	Log::Stream Log::fatal(std::source_location location) { return Stream(Level::Fatal, StringName(), location); }

	Log::Stream Log::fatal(const StringName& tag, std::source_location location)
	{
		return Stream(Level::Fatal, tag, location);
	}

	Log::Stream Log::fatal(std::string_view tag, std::source_location location)
	{
		return Stream(Level::Fatal, tag, location);
	}
}  // namespace meowody
