@tool
extends EditorExportPlugin
const Settings = preload("settings.gd")
var job := {}
var catalog := {}
var managed := {}
var allowed := {}

func _get_name() -> String:
	return "MeowodyResources"

func _export_begin(_features: PackedStringArray, _debug: bool, _path: String, _flags: int) -> void:
	job = Settings.read_json("res://.resource_export_job.json")
	allowed.clear()
	managed.clear()
	if not job.is_empty():
		for path in job.get("files", []): allowed[path] = true
		return
	catalog = Settings.read_json("res://resources/catalog.json")
	if catalog.is_empty():
		get_export_platform().add_message(EditorExportPlatform.EXPORT_MESSAGE_ERROR, "Resources", "Build addressable content for this platform before exporting the application")
		return
	var features := Array(_features)
	var target := "android" if "android" in features else "ios" if "ios" in features else "windows"
	if catalog.get("platform") != target:
		get_export_platform().add_message(EditorExportPlatform.EXPORT_MESSAGE_ERROR, "Resources", "Resource catalog platform does not match application export")
	var state := Settings.read_json("res://resources/export_state.json")
	for path in state.get("managed", []): managed[path] = true
	add_file("res://resources/catalog.json", JSON.stringify(catalog).to_utf8_buffer(), false)
	for pack in catalog.get("packs", {}).values():
		var local: String = pack.get("local", "")
		if not local.is_empty(): add_file(local, FileAccess.get_file_as_bytes(local), false)

func _export_file(path: String, _type: String, _features: PackedStringArray) -> void:
	if not job.is_empty():
		if not allowed.has(path): skip()
	elif managed.has(path) or path.begins_with("res://resources/") or path.begins_with("res://addons/meowody_resources/"):
		skip()
