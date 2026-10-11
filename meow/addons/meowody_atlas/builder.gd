@tool
extends RefCounted

const Packer = preload("packer.gd")
const Atlas = preload("ui_sprite_atlas.gd")
const OUTPUT := "res://generated/ui_atlases"
const CACHE := "res://.godot/meowody-atlas-cache"
const IMPORTER := "meowody.ui_sprite"


static func read_manifest(path: String) -> Dictionary:
	if not FileAccess.file_exists(path):
		return {}
	var data = JSON.parse_string(FileAccess.get_file_as_string(path))
	return data if data is Dictionary else {}


static func manifests() -> PackedStringArray:
	var result := PackedStringArray()
	var directory := DirAccess.open(OUTPUT)
	if directory != null:
		for folder in directory.get_directories():
			var path := OUTPUT.path_join(folder).path_join("manifest.json")
			if FileAccess.file_exists(path):
				result.append(path)
	result.sort()
	return result


static func manifest_path(atlas: Resource) -> String:
	return OUTPUT.path_join(atlas.atlas_id).path_join("manifest.json")


static func prepare(atlas: Resource) -> String:
	if not atlas.resource_path.begins_with("res://") or atlas.resource_path.contains("::"):
		return "Save the UISpriteAtlas resource before packing."
	if atlas.atlas_id.is_empty():
		atlas.atlas_id = ResourceUID.id_to_text(ResourceUID.create_id()).trim_prefix("uid://")
	if not ("atlas_" + str(atlas.atlas_id)).is_valid_identifier():
		return "Invalid atlas ID."
	var previous := read_manifest(manifest_path(atlas))
	if not previous.is_empty() and previous.asset != atlas.resource_path and FileAccess.file_exists(previous.asset):
		# A duplicated .tres must own a different output directory.
		atlas.atlas_id = ResourceUID.id_to_text(ResourceUID.create_id()).trim_prefix("uid://")
	var error := ResourceSaver.save(atlas)
	return "Cannot save atlas: " + error_string(error) if error != OK else ""


static func validate_profiles(profiles: Dictionary) -> String:
	for target in profiles:
		var mode: int = profiles[target].compression
		var allowed: Array = {"default": [0, 1], "desktop": [0, 1, 5], "android": [0, 1, 2, 3, 4],
			"ios": [0, 1, 2, 3, 4], "web": [0, 1]}[target]
		if not mode in allowed:
			return "Unsupported compression for %s. Use Lossless/Uncompressed as Default; select GPU formats in platform overrides." % target
	return ""


# Compression touches only Images and runs alongside packing on the worker thread.
static func build(settings: Dictionary, compress_pages := true) -> Dictionary:
	var error := validate_profiles(settings.profiles)
	if not error.is_empty():
		return {"error": error}
	var result := Packer.build(settings)
	if not result.error.is_empty() or not compress_pages:
		return result
	var variants := {}
	for target in settings.profiles:
		var profile: Dictionary = settings.profiles[target]
		var images: Array[Image] = []
		for page in result.pages:
			var image: Image = page.duplicate()
			if profile.compression >= 2:
				# Pad, never resize, so GPU block alignment cannot change sprite coordinates.
				var block := 8 if profile.compression == 4 else 4
				var size := Vector2i(ceili(float(image.get_width()) / block) * block, ceili(float(image.get_height()) / block) * block)
				var aligned := Image.create(size.x, size.y, false, Image.FORMAT_RGBA8)
				aligned.fill(Color.TRANSPARENT)
				aligned.blit_rect(image, Rect2i(Vector2i.ZERO, image.get_size()), Vector2i.ZERO)
				image = aligned
			if profile.mipmaps:
				image.generate_mipmaps()
			if profile.compression >= 2:
				var mode: Image.CompressMode = Image.COMPRESS_ETC2
				if profile.compression in [3, 4]:
					mode = Image.COMPRESS_ASTC
				elif profile.compression == 5:
					mode = Image.COMPRESS_BPTC
				error = error_string(image.compress_from_channels(mode, Image.USED_CHANNELS_RGBA,
					Image.ASTC_FORMAT_8x8 if profile.compression == 4 else Image.ASTC_FORMAT_4x4))
				if error != "OK":
					return {"error": "Compression failed for %s: %s" % [target, error]}
			images.append(image)
		variants[target] = images
	result.variants = variants
	return result


static func commit(atlas: Resource, result: Dictionary) -> Dictionary:
	if not ClassDB.class_exists("SpriteAtlas"):
		return {"error": "SpriteAtlas native backend is unavailable. Run scons and reload the GDExtension first."}
	var manifest_file := manifest_path(atlas)
	var folder := manifest_file.get_base_dir()
	var old := read_manifest(manifest_file)
	for other_path in manifests():
		if other_path == manifest_file:
			continue
		var other := read_manifest(other_path)
		for source in result.sprites:
			if other.get("sprites", {}).has(source):
				return {"error": "%s already belongs to %s. Remove it from that atlas first." % [source, other.asset]}
	var stage := "res://.godot/meowody-atlas-stage/%s" % Time.get_ticks_usec()
	if DirAccess.make_dir_recursive_absolute(stage) != OK:
		return {"error": "Cannot create atlas staging directory."}
	var writes := {}
	var files := []
	var pages := []
	var variants := {}
	for i in range(result.pages.size()):
		var path := folder.path_join("page_%d.tres" % i)
		var data_path := folder.path_join("data/page_%d.ctex" % i)
		var error := _stage_compressed_texture(result.pages[i], data_path, stage, writes, true)
		if error.is_empty():
			error = _stage_text("[gd_resource type=\"CompressedTexture2D\" format=3]\n\n[resource]\nload_path = %s\n" % JSON.stringify(data_path), path, stage, writes)
		if not error.is_empty():
			return {"error": error}
		files.append(path)
		files.append(data_path)
		pages.append(path)
	var ignore_path := folder.path_join("data/.gdignore")
	var ignore_error := _stage_text("", ignore_path, stage, writes)
	if not ignore_error.is_empty():
		return {"error": ignore_error}
	files.append(ignore_path)
	for target in result.variants:
		variants[target] = []
		for i in range(result.variants[target].size()):
			var path := CACHE.path_join(atlas.atlas_id).path_join("%s/page_%d.ctex" % [target, i])
			var error := _stage_compressed_texture(result.variants[target][i], path, stage, writes,
				result.settings.profiles[target].compression == 0)
			if not error.is_empty():
				return {"error": error}
			files.append(path)
			variants[target].append(path)
	var backups: Dictionary = old.get("import_backups", {}).duplicate()
	var reimport := PackedStringArray()
	var reserved := {}
	for source in old.get("sprites", {}):
		reserved[old.sprites[source].resource] = source
	for source in result.sprites:
		var entry: Dictionary = result.sprites[source]
		var sprite_path: String = old.get("sprites", {}).get(source, {}).get("resource", folder.path_join("sprites").path_join(source.trim_prefix("res://") + ".tres"))
		if reserved.has(sprite_path) and reserved[sprite_path] != source:
			sprite_path = sprite_path.get_basename() + "_" + str(source).sha256_text().left(12) + ".tres"
		reserved[sprite_path] = source
		entry.resource = sprite_path
		var header := "[gd_resource type=\"AtlasTexture\" load_steps=2 format=3 uid=\"%s\"]" % ResourceUID.id_to_text(ResourceUID.create_id())
		if FileAccess.file_exists(sprite_path):
			# Retain the Godot UID when existing scenes use it.
			header = FileAccess.get_file_as_string(sprite_path).get_slice("\n", 0)
		var text := header + "\n\n[ext_resource type=\"CompressedTexture2D\" path=%s id=\"1\"]\n\n[resource]\natlas = ExtResource(\"1\")\nregion = %s\nmargin = %s\nfilter_clip = true\n" % [
			JSON.stringify(pages[entry.page]), _rect_text(entry.region), _rect_text(entry.margin)]
		var error := _stage_text(text, sprite_path, stage, writes)
		if not error.is_empty():
			return {"error": error}
		files.append(sprite_path)
		var config := ConfigFile.new()
		config.load(source + ".import")
		if config.get_value("remap", "importer", "") != IMPORTER:
			backups[source] = config.encode_to_text()
		config.set_value("remap", "importer", IMPORTER)
		config.set_value("remap", "type", "AtlasTexture")
		# Background previews may load the source before EditorFileSystem reimports it.
		# Point at the already-published sprite during that transition, never old CTEX data.
		for key in config.get_section_keys("remap"):
			if str(key).begins_with("path"):
				config.erase_section_key("remap", key)
		config.set_value("remap", "path", sprite_path)
		if config.has_section("params"):
			config.erase_section("params")
		config.set_value("params", "atlas_manifest", manifest_file)
		error = _stage_text(config.encode_to_text(), source + ".import", stage, writes)
		if not error.is_empty():
			return {"error": error}
		reimport.append(source)
	var catalog_path := folder.path_join("atlas.tres")
	var catalog_header := "[gd_resource type=\"SpriteAtlas\" load_steps=%d format=3 uid=\"%s\"]" % [result.sprites.size() + 1, ResourceUID.id_to_text(ResourceUID.create_id())]
	if FileAccess.file_exists(catalog_path):
		var uid := ResourceLoader.get_resource_uid(catalog_path)
		if uid != ResourceUID.INVALID_ID:
			catalog_header = "[gd_resource type=\"SpriteAtlas\" load_steps=%d format=3 uid=\"%s\"]" % [result.sprites.size() + 1, ResourceUID.id_to_text(uid)]
	var catalog := catalog_header + "\n\n"
	var members := PackedStringArray()
	var sprite_index := 0
	for source in result.sprites:
		sprite_index += 1
		catalog += "[ext_resource type=\"AtlasTexture\" path=%s id=\"%d\"]\n" % [JSON.stringify(result.sprites[source].resource), sprite_index]
		members.append("%s: ExtResource(\"%d\")" % [JSON.stringify(str(source).trim_prefix("res://")), sprite_index])
	catalog += "\n[resource]\nsprites = {\n%s\n}\n" % ",\n".join(members)
	var catalog_error := _stage_text(catalog, catalog_path, stage, writes)
	if not catalog_error.is_empty():
		return {"error": catalog_error}
	files.append(catalog_path)
	var removals := PackedStringArray()
	for source in old.get("sprites", {}):
		if result.sprites.has(source):
			continue
		var error := _restore_import(source, backups.get(source, ""), stage, writes, reimport, removals)
		if not error.is_empty():
			return {"error": error}
		backups.erase(source)
	for path in old.get("files", []):
		if not path in files and _owned(path, folder):
			removals.append(path)
	var manifest := {"version": 2, "asset": atlas.resource_path, "sprites": result.sprites,
		"catalog": catalog_path, "pages": pages, "variants": variants, "files": files, "import_backups": backups,
		"fingerprint": result.fingerprint, "settings": result.settings}
	var error := _stage_text(JSON.stringify(manifest, "\t"), manifest_file, stage, writes)
	if error.is_empty():
		error = _publish(writes, removals, stage)
	if not error.is_empty():
		return {"error": error}
	# Refresh shared resources in place, so open scenes see new regions and pages.
	for path in pages:
		ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_REPLACE)
	for entry in result.sprites.values():
		ResourceLoader.load(entry.resource, "", ResourceLoader.CACHE_MODE_REPLACE)
	ResourceLoader.load(catalog_path, "", ResourceLoader.CACHE_MODE_REPLACE)
	return {"error": "", "reimport": reimport, "manifest": manifest}


static func remove_atlas(manifest_file: String, restore_only := false) -> Dictionary:
	var old := read_manifest(manifest_file)
	var stage := "res://.godot/meowody-atlas-stage/%s" % Time.get_ticks_usec()
	DirAccess.make_dir_recursive_absolute(stage)
	var writes := {}
	var reimport := PackedStringArray()
	var removals := PackedStringArray()
	if not restore_only:
		removals.append(manifest_file)
		for path in old.get("files", []):
			if _owned(path, manifest_file.get_base_dir()):
				removals.append(path)
	for source in old.get("sprites", {}):
		var error := _restore_import(source, old.get("import_backups", {}).get(source, ""), stage, writes, reimport, removals)
		if not error.is_empty():
			return {"error": error}
	return {"error": _publish(writes, removals, stage), "reimport": reimport}


static func _owned(path: String, folder: String) -> bool:
	return folder.begins_with(OUTPUT + "/") and (path.simplify_path().begins_with(folder + "/") or path.simplify_path().begins_with(CACHE.path_join(folder.get_file()) + "/"))


static func _restore_import(source: String, backup: String, stage: String, writes: Dictionary,
		reimport: PackedStringArray, removals: PackedStringArray) -> String:
	var config := ConfigFile.new()
	if config.load(source + ".import") == OK and config.get_value("remap", "importer", "") != IMPORTER:
		return ""
	if not FileAccess.file_exists(source):
		removals.append(source + ".import")
		return ""
	config.parse(backup)
	if config.get_value("remap", "importer", "") in ["", IMPORTER]:
		config.set_value("remap", "importer", "texture")
		config.set_value("remap", "type", "CompressedTexture2D")
		if config.has_section("params"):
			config.erase_section("params")
	reimport.append(source)
	return _stage_text(config.encode_to_text(), source + ".import", stage, writes)


static func _rect_text(values: Array) -> String:
	return "Rect2(%d, %d, %d, %d)" % values


static func _stage_text(contents: String, path: String, stage: String, writes: Dictionary) -> String:
	var temporary := stage.path_join("%d.tmp" % writes.size())
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	if file == null:
		return "Cannot stage " + path
	file.store_string(contents)
	file.flush()
	if file.get_error() != OK:
		return "Cannot write " + path
	writes[path] = temporary
	return ""


static func _publish(writes: Dictionary, removals: PackedStringArray, stage: String) -> String:
	var targets := writes.keys()
	for path in removals:
		if not path in targets:
			targets.append(path)
	var originals := {}
	var published := []
	var error := OK
	for path in targets:
		error = DirAccess.make_dir_recursive_absolute(str(path).get_base_dir())
		if error != OK:
			break
		if FileAccess.file_exists(path):
			var backup := stage.path_join("old_%d" % originals.size())
			error = DirAccess.rename_absolute(path, backup)
			if error != OK:
				break
			originals[path] = backup
		if writes.has(path):
			error = DirAccess.rename_absolute(writes[path], path)
			if error != OK:
				break
			published.append(path)
	if error != OK:
		for path in published:
			DirAccess.remove_absolute(path)
		for path in originals:
			if DirAccess.rename_absolute(originals[path], path) != OK:
				return "Atlas publish failed. Recovery files remain in " + stage
		return "Atlas publish failed; previous files restored: " + error_string(error)
	for backup in originals.values():
		DirAccess.remove_absolute(backup)
	DirAccess.remove_absolute(stage)
	return ""


# Native GST2 container. Write CPU image bytes without uploading mobile GPU formats.
static func _stage_compressed_texture(image: Image, path: String, stage: String, writes: Dictionary, lossless: bool) -> String:
	var temporary := stage.path_join("%d.ctex" % writes.size())
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	if file == null:
		return "Cannot stage " + path
	file.store_buffer("GST2".to_ascii_buffer())
	file.store_32(1)
	file.store_32(image.get_width())
	file.store_32(image.get_height())
	file.store_32((1 << 23) if image.has_mipmaps() else 0)
	file.store_32(0xffffffff)
	for i in range(3):
		file.store_32(0)
	file.store_32(1 if lossless else 0)
	file.store_16(image.get_width())
	file.store_16(image.get_height())
	file.store_32(image.get_mipmap_count())
	file.store_32(image.get_format())
	var data := image.get_data()
	if lossless:
		for level in range(image.get_mipmap_count() + 1):
			var width := maxi(1, image.get_width() >> level)
			var height := maxi(1, image.get_height() >> level)
			var offset := image.get_mipmap_offset(level)
			var mip := Image.create_from_data(width, height, false, image.get_format(), data.slice(offset, offset + width * height * 4))
			var png := "PNG ".to_ascii_buffer() + mip.save_png_to_buffer()
			file.store_32(png.size())
			file.store_buffer(png)
	else:
		file.store_buffer(data)
	file.flush()
	if file.get_error() != OK:
		return "Cannot write " + path
	writes[path] = temporary
	return ""


static func cache_ready(manifest: Dictionary) -> bool:
	if manifest.get("version", 0) != 2 or not FileAccess.file_exists(manifest.get("catalog", "")):
		return false
	for paths in manifest.get("variants", {}).values():
		for path in paths:
			if not FileAccess.file_exists(path):
				return false
	return true


static func editor_page_image(page_path: String) -> Image:
	var data_path := page_path.get_base_dir().path_join("data").path_join(page_path.get_file().get_basename() + ".ctex")
	if FileAccess.file_exists(data_path):
		# Editor pages always contain a lossless RGBA8 PNG, prefixed by Godot's PNG tag.
		var file := FileAccess.open(data_path, FileAccess.READ)
		if file == null:
			return null
		file.seek(52)
		var size := file.get_32()
		var image := Image.new()
		if image.load_png_from_buffer(file.get_buffer(size).slice(4)) == OK:
			return image
		return null
	# Compatibility with v1 ImageTexture pages until the next successful Pack.
	var texture: Texture2D = load(page_path)
	return texture.get_image() if texture != null else null
