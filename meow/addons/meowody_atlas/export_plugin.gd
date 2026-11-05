@tool
extends EditorExportPlugin

const Builder = preload("builder.gd")
var manager: EditorPlugin
var replacements := {}
var platform_name := "desktop"
var content_job := {}
var managed_content := []

func _get_name() -> String:
	return "MeowodyUISpriteAtlas"

func _export_begin(features: PackedStringArray, _debug: bool, _path: String, _flags: int) -> void:
	replacements.clear()
	content_job.clear()
	managed_content.clear()
	if FileAccess.file_exists("res://.resource_export_job.json"):
		var job = JSON.parse_string(FileAccess.get_file_as_string("res://.resource_export_job.json"))
		if job is Dictionary: content_job = job
	elif FileAccess.file_exists("res://resources/export_state.json"):
		var state = JSON.parse_string(FileAccess.get_file_as_string("res://resources/export_state.json"))
		if state is Dictionary: managed_content = state.get("managed", [])
	platform_name = target_for(features)
	var error: String = manager.prepare_export()
	if not error.is_empty():
		get_export_platform().add_message(EditorExportPlatform.EXPORT_MESSAGE_ERROR, "UI Sprite Atlas", error)
		return
	for path in Builder.manifests():
		var manifest := Builder.read_manifest(path)
		var variant: Array = manifest.variants.get(platform_name, manifest.variants.get("default", []))
		for i in range(manifest.pages.size()):
			replacements[manifest.pages[i]] = variant[i]

func _export_file(path: String, _type: String, _features: PackedStringArray) -> void:
	# add_file bypasses other exporters' skip(); filter generated atlas payloads here.
	if (not content_job.is_empty() and path not in content_job.get("files", [])) or path in managed_content:
		skip()
		return
	if replacements.has(path):
		var data_path := path.get_base_dir().path_join("data").path_join(path.get_file().get_basename() + ".ctex")
		add_file(data_path, FileAccess.get_file_as_bytes(replacements[path]), false)
	elif path.begins_with(Builder.OUTPUT + "/") and (path.contains("/platforms/") or path.ends_with("manifest.json")):
		skip()

static func target_for(features: PackedStringArray) -> String:
	if "android" in features: return "android"
	if "ios" in features: return "ios"
	if "web" in features: return "web"
	return "desktop"
