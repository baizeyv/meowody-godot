#include "register_types.h"

#include <gdextension_interface.h>

#include "runtime/app.h"
#include "editor_registration.h"
#include "log/crash_handler.h"

using namespace godot;
using namespace meowody;

void initialize_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		initialize_editor_module();
	}

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	register_generated_types();
	CrashHandler::initialize();
	App::initialize();
}

void uninitialize_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		uninitialize_editor_module();
		return;
	}

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	App::finalize();
	CrashHandler::shutdown();
}

extern "C" {
GDExtensionBool GDE_EXPORT meowody_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_module);
	init_obj.register_terminator(uninitialize_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
