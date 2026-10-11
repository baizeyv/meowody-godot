extends SceneTree
func _initialize() -> void:
    var status := GDExtensionManager.load_extension("res://bin/meowody.gdextension")
    if status != GDExtensionManager.LOAD_STATUS_OK and status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
        quit(1)
        return
    var probe = ClassDB.instantiate("CrashProbe")
    if not probe.handled_fault():
        quit(2)
        return
    var directory := DirAccess.open("user://logs")
    if directory:
        for name in directory.get_files():
            if name.begins_with("crash_"):
                push_error("Handled exception incorrectly produced a crash report")
                quit(3)
                return
    print("[CrashTest] handled exception produced no report")
    probe.crash()
    quit(4)
