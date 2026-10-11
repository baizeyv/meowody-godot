#pragma once
#include "save_types.h"
namespace meowody::detail {
// Both files must be in the same directory. Never deletes the destination first.
SaveStatus replace_durable(const godot::String& temporary, const godot::String& destination);
}
