#include <godot_cpp/classes/time.hpp>
#include <memory>

#include "log.hpp"
#include "runtime/event/event_channel.hpp"

namespace meowody {
namespace {
std::unique_ptr<EventChannel<LogRecord>> records;
uint64_t sequence = 0;
bool capture = true;
int minimum = 0;
thread_local bool publishing = false;
} // namespace
Connection Log::subscribe(Delegate<void(const LogRecord &)> callback) {
	if (!records)
		records = std::make_unique<EventChannel<LogRecord>>();
	return records->connect(std::move(callback));
}
void Log::set_stack_capture(bool enabled, int level) {
	capture = enabled;
	minimum = level;
}
void detail::publish_log(int level, const godot::String &tag,
		const godot::String &text,
		std::source_location location) {
	if (publishing || !records || records->empty())
		return;
	publishing = true;
	struct Guard {
		~Guard() { publishing = false; }
	} guard;
	LogRecord record;
	record.sequence = ++sequence;
	record.timestamp =
			godot::Time::get_singleton()->get_unix_time_from_system();
	record.rich = Log::is_rich_enabled();
	record.level = level;
	record.tag = tag;
	record.text = text;
	if (capture && level >= minimum) {
		record.stack.push_back(godot::String("Logged at ") +
				location.file_name() + ":" +
				godot::String::num_uint64(location.line()) +
				"  " + location.function_name());
		record.stack.append_array(capture_log_stack());
	}
	records->emit(record);
}
void detail::shutdown_log_records() {
	records.reset();
}
} // namespace meowody
