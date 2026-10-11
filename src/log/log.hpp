//
// Created by baizeyv on 8/30/2026.
//

#pragma once
#include "log_record.h"
#include <source_location>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <ostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

using namespace godot;

namespace meowody
{
	class Log
	{
	  public:
		static Connection subscribe(Delegate<void(const LogRecord&)> callback);
		static void set_stack_capture(bool enabled, int minimum_level = 0);
		// ========================================================================
		// Level
		// ========================================================================

		enum class Level : uint8_t
		{
			Debug = 0,
			Info,
			Warn,
			Error,
			Fatal
		};

		// ========================================================================
		// Stream
		// ========================================================================

		class Stream
		{
		  public:
			Stream(Level level, const StringName& tag, std::source_location location = std::source_location::current());

			Stream(Level level, std::string_view tag, std::source_location location = std::source_location::current());

			~Stream();

			Stream(const Stream&)			 = delete;
			Stream& operator=(const Stream&) = delete;

			Stream(Stream&& other) noexcept;

			Stream& operator=(Stream&& other) noexcept;

			// --------------------------------------------------------------------
			// C++ types
			// --------------------------------------------------------------------

			template <typename T>
			Stream& operator<<(const T& value)
			{
				if (_enabled)
				{
					_stream << value;
				}

				return *this;
			}

			// --------------------------------------------------------------------
			// std::endl / std::flush
			// --------------------------------------------------------------------

			using Manipulator = std::ostream& (*)(std::ostream&);

			Stream& operator<<(Manipulator manipulator);

			// --------------------------------------------------------------------
			// Godot types
			// --------------------------------------------------------------------

			Stream& operator<<(const String& value);

			Stream& operator<<(const StringName& value);

			Stream& operator<<(const Variant& value);

			Stream& operator<<(const Object* object);

			// --------------------------------------------------------------------
			// Flush
			// --------------------------------------------------------------------

			void flush();

			// --------------------------------------------------------------------
			// State
			// --------------------------------------------------------------------

			bool enabled() const { return _enabled; }

		  private:
			Level _level;
            std::source_location _location;

			StringName _tag;

			std::ostringstream _stream;

			bool _enabled = false;

			bool _flushed = false;
		};

		// ========================================================================
		// Global configuration
		// ========================================================================

		/**
		 * 设置全局最低日志等级。
		 *
		 * 例如：
		 *
		 * Log::set_level(Log::Level::Info);
		 *
		 * Debug 将不会输出。
		 */
		static void set_level(Level level);

		static Level get_level();

		/**
		 * 判断某个 Level 是否启用。
		 *
		 * 用于避免昂贵计算：
		 *
		 * if (Log::is_enabled(Log::Level::Debug)) {
		 *     Log::debug() << GenerateHugeData();
		 * }
		 */
		static bool is_enabled(Level level);

		/**
		 * Rich Text 输出。
		 *
		 * true:
		 *     UtilityFunctions::print_rich()
		 *
		 * false:
		 *     UtilityFunctions::print()
		 */
		static void set_rich_enabled(bool enabled);

		static bool is_rich_enabled();

		/**
		 * 设置默认 Tag。
		 *
		 * 默认：
		 *
		 * Game
		 */
		static void set_default_tag(const StringName& tag);

		static const StringName& get_default_tag();
		// Release engine-backed values before GDExtension teardown.
		static void shutdown();

		// ========================================================================
		// Tag configuration
		// ========================================================================

		/**
		 * 为指定 Tag 设置最低日志等级。
		 *
		 * 例如：
		 *
		 * Log::set_tag_level(
		 *     "EventCenter",
		 *     Log::Level::Debug
		 * );
		 *
		 * 即使全局是 Info：
		 *
		 * Log::set_level(Log::Level::Info);
		 *
		 * EventCenter 仍然允许 Debug。
		 */
		static void set_tag_level(const StringName& tag, Level level);

		/**
		 * 删除 Tag 的独立 Level 设置。
		 *
		 * 删除后重新使用全局 Level。
		 */
		static void reset_tag_level(const StringName& tag);

		/**
		 * 获取 Tag 当前使用的 Level。
		 *
		 * 如果没有独立设置，则返回全局 Level。
		 */
		static Level get_tag_level(const StringName& tag);

		/**
		 * 开启 / 关闭指定 Tag。
		 */
		static void set_tag_enabled(const StringName& tag, bool enabled);

		/**
		 * 判断 Tag 是否开启。
		 */
		static bool is_tag_enabled(const StringName& tag);

		/**
		 * 清除所有 Tag 配置。
		 */
		static void clear_tag_settings();

		// ========================================================================
		// Direct print
		// ========================================================================

		static void print(Level level, const String& message,
						  const StringName& tag = StringName(), std::source_location location = std::source_location::current());

		static void print(Level level, std::string_view message,
						  const StringName& tag = StringName(), std::source_location location = std::source_location::current());

		// ========================================================================
		// Stream API
		// ========================================================================

		static Stream debug(std::source_location location = std::source_location::current());

		static Stream debug(const StringName& tag, std::source_location location = std::source_location::current());

		static Stream debug(std::string_view tag, std::source_location location = std::source_location::current());

		static Stream info(std::source_location location = std::source_location::current());

		static Stream info(const StringName& tag, std::source_location location = std::source_location::current());

		static Stream info(std::string_view tag, std::source_location location = std::source_location::current());

		static Stream warn(std::source_location location = std::source_location::current());

		static Stream warn(const StringName& tag, std::source_location location = std::source_location::current());

		static Stream warn(std::string_view tag, std::source_location location = std::source_location::current());

		static Stream error(std::source_location location = std::source_location::current());

		static Stream error(const StringName& tag, std::source_location location = std::source_location::current());

		static Stream error(std::string_view tag, std::source_location location = std::source_location::current());

		static Stream fatal(std::source_location location = std::source_location::current());

		static Stream fatal(const StringName& tag, std::source_location location = std::source_location::current());

		static Stream fatal(std::string_view tag, std::source_location location = std::source_location::current());

	  private:
		// ========================================================================
		// Internal
		// ========================================================================

		static bool should_log(Level level, const StringName& tag);

		static const char* level_name(Level level);

		static const char* level_color(Level level);

		static String make_message(Level level, const StringName& tag,
								   std::string_view message);

	  private:
		static Level _level;

		static bool _rich_enabled;

		static std::optional<StringName> _default_tag;
	};
}  // namespace meowody
