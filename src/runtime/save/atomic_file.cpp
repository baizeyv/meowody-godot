#include "atomic_file.h"

#include <godot_cpp/classes/dir_access.hpp>

namespace meowody::detail
{
	SaveStatus replace_durable(const godot::String& temporary,
							   const godot::String& destination)
	{
		if (temporary.simplify_path().get_base_dir() !=
			destination.simplify_path().get_base_dir())
			return {SaveError::IO,
					"Atomic replacement requires the same directory"};
		// save_store closes and flushes the temporary file before calling here.
		// Never remove the original first to force a successful rename.
		if (godot::DirAccess::rename_absolute(temporary, destination) !=
			godot::OK)
			return {SaveError::IO, "Godot file replacement failed"};
		// No fsync/FlushFileBuffers power-loss durability in this mode.
		return {};
	}
}  // namespace meowody::detail
