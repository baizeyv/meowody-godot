#include "binding_editor_backend.h"
#include "binding_generator.h"
#include <exception>

namespace meowody {
using namespace godot;
namespace {
Dictionary result(const BindingResult &value) {
	Dictionary out;
	out["ok"] = value.ok;
	out["error"] = value.error;
	return out;
}
}

void BindingEditorBackend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("default_class_name", "root"), &BindingEditorBackend::default_class_name);
	ClassDB::bind_method(D_METHOD("valid_class_name", "name"), &BindingEditorBackend::valid_class_name);
	ClassDB::bind_method(D_METHOD("describe_scene", "root"), &BindingEditorBackend::describe_scene);
	ClassDB::bind_method(D_METHOD("generate_scene", "root"), &BindingEditorBackend::generate_scene);
}
String BindingEditorBackend::default_class_name(Node *root) const {
	return BindingGenerator::default_class_name(root);
}
bool BindingEditorBackend::valid_class_name(const String &name) const {
	return BindingGenerator::valid_class_name(name);
}
Dictionary BindingEditorBackend::describe_scene(Node *root) const {
	String error;
	auto bindings = BindingGenerator::collect(root, error);
	auto out = result({error.is_empty(), error});
	Array rows;
	for (const auto &binding : bindings) {
		Dictionary row;
		row["path"] = root->get_path_to(binding.node);
		row["member"] = binding.member;
		row["type"] = binding.cpp_type;
		rows.append(row);
	}
	out["bindings"] = rows;
	return out;
}
Dictionary BindingEditorBackend::generate_scene(Node *root) const {
	try {
		return result(BindingGenerator::generate(root));
	} catch (const std::exception &error) {
		return result({false, String("Generation failed: ") + error.what()});
	}
}
} // namespace meowody
