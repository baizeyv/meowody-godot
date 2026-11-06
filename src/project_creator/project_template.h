#pragma once
#include <godot_cpp/variant/dictionary.hpp>

namespace creator {
// Main thread only: engine JSON services are not used by background jobs.
godot::Dictionary generate_project(const godot::Dictionary &request);
}
