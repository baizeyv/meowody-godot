@tool
extends EditorImportPlugin

const Builder = preload("builder.gd")

func _get_importer_name() -> String: return Builder.IMPORTER
func _get_visible_name() -> String: return "UI Sprite Atlas"
func _get_recognized_extensions() -> PackedStringArray: return PackedStringArray(preload("source_images.gd").EXTENSIONS)
func _get_save_extension() -> String: return "res"
func _get_resource_type() -> String: return "AtlasTexture"
func _get_priority() -> float: return 0.0
func _get_import_order() -> int: return 100
func _get_preset_count() -> int: return 1
func _get_preset_name(_index: int) -> String: return "Atlas managed"
func _get_import_options(_path: String, _preset_index: int) -> Array[Dictionary]:
	return [{"name": "atlas_manifest", "default_value": "", "usage": PROPERTY_USAGE_STORAGE}]

func _import(source_file: String, save_path: String, options: Dictionary,
		_platform_variants: Array[String], _gen_files: Array[String]) -> Error:
	var manifest := Builder.read_manifest(options.get("atlas_manifest", ""))
	var entry: Dictionary = manifest.get("sprites", {}).get(source_file, {})
	if entry.is_empty() or not FileAccess.file_exists(entry.get("resource", "")):
		push_error("UI atlas needs packing before importing " + source_file)
		return ERR_FILE_MISSING_DEPENDENCIES
	var sprite: AtlasTexture = ResourceLoader.load(entry.resource, "", ResourceLoader.CACHE_MODE_REPLACE)
	if sprite == null:
		return ERR_CANT_OPEN
	# Save a distinct resource, retaining an external reference to the shared page.
	return ResourceSaver.save(sprite.duplicate(), save_path + ".res")
