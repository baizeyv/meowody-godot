#pragma once

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

namespace meowody::reflection {
// Categories carry no value or accessors. ClassDB::add_property requires a getter,
// so register this editor-only marker through the extension interface directly.
inline void add_category(const godot::StringName &class_name, const godot::String &name) {
    const godot::PropertyInfo info(godot::Variant::NIL, name, godot::PROPERTY_HINT_NONE,
            "", godot::PROPERTY_USAGE_CATEGORY);
    const auto native = info._to_gdextension();
    const godot::StringName empty;
    godot::gdextension_interface::classdb_register_extension_class_property_indexed(
            godot::gdextension_interface::library, class_name._native_ptr(), &native,
            empty._native_ptr(), empty._native_ptr(), -1);
}
}
