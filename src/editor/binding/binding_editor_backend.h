#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace meowody {
// Short-lived calls only. The GDScript editor owns the build/reload operation.
class BindingEditorBackend : public godot::RefCounted {
	GDCLASS(BindingEditorBackend, godot::RefCounted)

protected:
	static void _bind_methods();

public:
	godot::String default_class_name(godot::Node *root) const;
	bool valid_class_name(const godot::String &name) const;
	godot::Dictionary describe_scene(godot::Node *root) const;
	godot::Dictionary generate_scene(godot::Node *root) const;
};
} // namespace meowody
