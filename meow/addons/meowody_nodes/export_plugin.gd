@tool
extends EditorExportPlugin
func _get_name() -> String:
	return "MeowodyCommonNodesShaders"
func _export_begin(_features: PackedStringArray, _debug: bool, _path: String, _flags: int) -> void:
	# C++ loads these by path, so selected-scene exports cannot discover them as dependencies.
	for name in ["gradient_rect", "shape_rect"]:
		var path: String = "res://addons/meowody_nodes/shaders/" + name + ".gdshader"
		add_file(path, FileAccess.get_file_as_bytes(path), false)
