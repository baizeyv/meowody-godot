#include "editor_registration.h"

#include "binding/binding_editor_backend.h"
#include "debug/debug_editor_backend.h"

namespace meowody {

void initialize_editor_module() {
	using namespace godot;
	ClassDB::register_class<DebugEditorBackend>();
	DebugEditorBackend::initialize();
	ClassDB::register_class<BindingEditorBackend>();
}

void uninitialize_editor_module() {
	DebugEditorBackend::shutdown();
}

} // namespace meowody
