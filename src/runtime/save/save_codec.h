#pragma once
#include "save_types.h"
#include <godot_cpp/classes/object.hpp>
namespace meowody::detail {
bool safe_variant(const godot::Variant& value, int depth = 0);
SaveResult export_properties(godot::Object* object);
// Validates the complete property tree before applying any setters.
SaveStatus import_properties(godot::Object* object, const godot::Dictionary& properties);
godot::PackedByteArray digest(const godot::PackedByteArray& bytes);
}
