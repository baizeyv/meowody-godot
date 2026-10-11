@tool
extends EditorDebuggerPlugin
signal records_received(rows: Array, source: String)

func _has_capture(prefix: String) -> bool:
    return prefix == "meowody_log"

func _capture(message: String, data: Array, session_id: int) -> bool:
    if message != "meowody_log:records":
        return false
    records_received.emit(data.slice(0, 128), "Game session %s" % session_id)
    return true
