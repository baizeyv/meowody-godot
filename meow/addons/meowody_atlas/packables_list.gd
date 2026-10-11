@tool
extends ItemList

signal paths_dropped(paths: PackedStringArray)

func _can_drop_data(_position: Vector2, data: Variant) -> bool:
	if not data is Dictionary or data.get("type", "") != "files":
		return false
	for path in data.get("files", []):
		if not preload("source_images.gd").supports(str(path)) and not DirAccess.dir_exists_absolute(path):
			return false
	return not data.get("files", []).is_empty()

func _drop_data(_position: Vector2, data: Variant) -> void:
	paths_dropped.emit(PackedStringArray(data.files))
