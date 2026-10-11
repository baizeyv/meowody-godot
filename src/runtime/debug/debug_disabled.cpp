#include "debug_center.h"
namespace meowody
{
	// Shipping API stub: no UI, command registration, log capture, transports
	// or stacks.
	DebugCenter::DebugCenter()	= default;
	DebugCenter::~DebugCenter() = default;
	void DebugCenter::set_enabled(bool) {}
	bool DebugCenter::captures_input() const { return false; }
	void DebugCenter::open() {}
	void DebugCenter::close() {}
	bool DebugCenter::register_command(DebugCommand&&) { return false; }
	bool DebugCenter::unregister_command(const godot::String&) { return false; }
	bool DebugCenter::execute_command(const godot::String&) { return false; }
	godot::PackedStringArray DebugCenter::complete(const godot::String&) const
	{
		return {};
	}
	void DebugCenter::print(const godot::String&, bool) {}
	bool DebugCenter::register_panel(const godot::String&, const godot::String&,
									 Delegate<godot::Control*()>)
	{
		return false;
	}
	bool DebugCenter::unregister_panel(const godot::String&) { return false; }
	godot::String DebugCenter::stats() const
	{
		return "Debug console excluded from this build";
	}
	godot::String DebugCenter::export_save(const godot::String&,
										   const godot::String&, bool, bool)
	{
		return "Debug console excluded from this build";
	}
	void DebugCenter::update(double) {}
	void DebugCenter::shutdown() {}
}  // namespace meowody
