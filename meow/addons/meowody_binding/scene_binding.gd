@tool
extends RefCounted


# Keep temporary scene/resource ownership in the editor, outside the reloadable DLL.
static func apply(path: String, native_class: String, bindings: Array, backup: String) -> String:
	var source := FileAccess.get_file_as_string(path)
	var start := source.find("[node ")
	var end := source.find("]", start)
	if start < 0 or end < 0:
		return "Cannot locate the scene root."
	var header := source.substr(start, end - start + 1)
	var type := header.find('type="')
	if type < 0:
		return "Open the original source scene instead of an inherited scene."
	var value_start := type + 6
	var value_end := header.find('"', value_start)
	header = header.substr(0, value_start) + native_class + header.substr(value_end)
	var temporary := path.get_basename() + ".binding-tmp.tscn"
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	if file == null:
		return "Cannot write the temporary scene."
	file.store_string(source.substr(0, start) + header + source.substr(end + 1))
	file = null
	var converted: PackedScene = ResourceLoader.load(temporary, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if converted == null:
		DirAccess.remove_absolute(temporary)
		return "Cannot load the converted scene."
	var root := converted.instantiate()
	var error := _assign(root, native_class, bindings)
	var packed := PackedScene.new()
	if error.is_empty():
		root.scene_file_path = path
		if packed.pack(root) != OK or ResourceSaver.save(packed, temporary) != OK:
			error = "Cannot save the bound scene."
	if root != null:
		root.free()
	if not error.is_empty():
		DirAccess.remove_absolute(temporary)
		return error
	if DirAccess.rename_absolute(path, backup) != OK:
		DirAccess.remove_absolute(temporary)
		return "Cannot back up the original scene."
	if DirAccess.rename_absolute(temporary, path) != OK:
		var restored := DirAccess.rename_absolute(backup, path)
		DirAccess.remove_absolute(temporary)
		return "Cannot replace the scene. Restore result: %s. Backup: %s" % [error_string(restored), backup]
	return ""


static func _assign(root: Node, native_class: String, bindings: Array) -> String:
	if root == null or root.get_class() != native_class:
		return "The converted scene does not have the expected native root: " + native_class
	var properties := {}
	for property in root.get_property_list():
		properties[property.name] = property
	if root.has_meta("meowody_ui_animation_property"):
		root.remove_meta("meowody_ui_animation_property")
	for binding in bindings:
		var node := root.get_node_or_null(binding.path)
		var property: Dictionary = properties.get(binding.member, {})
		var type: String = property.get("class_name", "")
		if node == null or property.get("type") != TYPE_OBJECT or (not type.is_empty() and not node.is_class(type)):
			return "Missing or incompatible exported member: " + binding.member
		root.set(binding.member, node)
		if root.get(binding.member) != node:
			return "Could not assign node reference: " + binding.member
		if node.get_meta("meowody_ui_animation", false):
			root.set_meta("meowody_ui_animation_property", binding.member)
	return ""
