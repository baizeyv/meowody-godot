@tool
extends RefCounted

const Builder = preload("builder.gd")

# Resource.reload_from_file cannot change a CompressedTexture2D into an AtlasTexture.
# Detach only that one-time type transition before EditorFileSystem reloads the PNG.
static func detach_type_changes(paths: PackedStringArray) -> Dictionary:
	var detached := {}
	for path in paths:
		var cached := ResourceLoader.get_cached_ref(path)
		if not cached is Texture2D:
			continue
		var config := ConfigFile.new()
		config.load(path + ".import")
		var expects_atlas: bool = config.get_value("remap", "importer", "") == Builder.IMPORTER
		if (cached is AtlasTexture) != expects_atlas:
			detached[cached] = path
			cached.resource_path = ""
	return detached


static func reconnect(detached: Dictionary) -> void:
	if detached.is_empty():
		return
	var replacements := {}
	for old in detached:
		var texture := ResourceLoader.load(detached[old], "", ResourceLoader.CACHE_MODE_REPLACE)
		if texture != null:
			replacements[old] = texture
		else:
			old.take_over_path(detached[old])
	var visited := {}
	for root in EditorInterface.get_open_scene_roots():
		_replace_object(root, replacements, visited)
	var edited := EditorInterface.get_inspector().get_edited_object()
	if edited is Resource:
		_replace_object(edited, replacements, visited)


static func _replace_object(object: Object, replacements: Dictionary, visited: Dictionary) -> void:
	if object == null or visited.has(object.get_instance_id()) or object is Script:
		return
	visited[object.get_instance_id()] = true
	for property in object.get_property_list():
		if not property.usage & PROPERTY_USAGE_STORAGE:
			continue
		var value = object.get(property.name)
		var replaced = _replace_value(value, replacements, visited)
		if not is_same(value, replaced):
			object.set(property.name, replaced)
	if object is Node:
		for child in object.get_children():
			_replace_object(child, replacements, visited)


static func _replace_value(value: Variant, replacements: Dictionary, visited: Dictionary) -> Variant:
	if value is Resource:
		if replacements.has(value):
			return replacements[value]
		_replace_object(value, replacements, visited)
	elif value is Array or value is Dictionary:
		# Containers may be nested, but resource cycles are guarded by instance ID.
		var copy = value.duplicate()
		var changed := false
		for key in (range(value.size()) if value is Array else value.keys()):
			var next = _replace_value(value[key], replacements, visited)
			if not is_same(value[key], next):
				copy[key] = next
				changed = true
		if changed:
			return copy
	return value
