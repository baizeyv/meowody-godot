#include "log_record.h"

namespace meowody::detail {
// Portable default; the emitter still records its C++20 source_location.
// The optional Windows backend replaces this translation unit at build time.
godot::PackedStringArray capture_log_stack() { return {}; }
}
