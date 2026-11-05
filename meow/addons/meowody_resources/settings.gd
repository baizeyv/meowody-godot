@tool
extends RefCounted

const PATH := "res://resources/settings.json"

static func defaults() -> Dictionary:
	return {"schema": 1, "compatibility": "1", "active_profile": "development", "play_mode": "source",
		"groups": [{"name": "Local", "remote": false, "startup": true}], "entries": [],
		"profiles": {"development": {"remote_root": "http://127.0.0.1:8080", "output": "../build/resources/development", "presets": {"windows": "Windows Desktop", "android": "Android", "ios": "iOS"}},
		"testing": {"remote_root": "", "output": "../build/resources/testing", "presets": {"windows": "Windows Desktop", "android": "Android", "ios": "iOS"}},
		"production": {"remote_root": "", "output": "../build/resources/production", "presets": {"windows": "Windows Desktop", "android": "Android", "ios": "iOS"}}}}

static func read_json(path: String) -> Dictionary:
	if not FileAccess.file_exists(path): return {}
	var value = JSON.parse_string(FileAccess.get_file_as_string(path))
	return value if value is Dictionary else {}

static func load_settings() -> Dictionary:
	return read_json(PATH) if FileAccess.file_exists(PATH) else defaults()

static func save_json(path: String, value: Dictionary) -> Error:
	var error := DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	if error != OK: return error
	var file := FileAccess.open(path + ".tmp", FileAccess.WRITE)
	if file == null: return FileAccess.get_open_error()
	file.store_string(JSON.stringify(value, "\t", true) + "\n")
	file.close()
	return DirAccess.rename_absolute(path + ".tmp", path)

static func resolve(settings: Dictionary) -> PackedStringArray:
	var errors := PackedStringArray()
	if settings.get("schema") != 1 or not settings.get("entries") is Array or not settings.get("groups") is Array:
		return PackedStringArray(["Invalid resource settings schema"])
	var groups := {}
	for group in settings.groups:
		if not group is Dictionary or str(group.get("name", "")).is_empty() or groups.has(group.name):
			errors.append("Invalid or duplicate group")
			continue
		groups[group.name] = group
	var addresses := {}
	var paths := {}
	for entry in settings.entries:
		if not entry is Dictionary:
			errors.append("Invalid entry")
			continue
		var address := str(entry.get("address", ""))
		var path := str(entry.get("path", ""))
		var uid := ResourceUID.text_to_id(str(entry.get("uid", "")))
		if ResourceUID.has_id(uid):
			path = ResourceUID.get_id_path(uid)
			entry.path = path
		if address.is_empty() or address.contains("://") or addresses.has(address): errors.append("Invalid or duplicate address: " + address)
		if paths.has(path): errors.append("Resource registered twice: " + path)
		if not path.begins_with("res://") or not ResourceLoader.exists(path): errors.append("Missing resource: " + path)
		if not groups.has(entry.get("group", "")): errors.append("Unknown group: " + address)
		if not entry.get("labels", []) is Array: errors.append("Invalid labels: " + address)
		addresses[address] = true
		paths[path] = true
	return errors
