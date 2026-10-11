#include "log.hpp"

namespace meowody {

Connection Log::subscribe(Delegate<void(const LogRecord &)>) {
	return {};
}

void Log::set_stack_capture(bool, int) {}

void detail::publish_log(int, const godot::String &, const godot::String &,
		std::source_location) {}

void detail::shutdown_log_records() {}

} // namespace meowody