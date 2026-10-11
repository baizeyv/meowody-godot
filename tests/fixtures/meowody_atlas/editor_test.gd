@tool
extends RefCounted

const Atlas = preload("res://addons/meowody_atlas/ui_sprite_atlas.gd")
const Builder = preload("res://addons/meowody_atlas/builder.gd")
const Packer = preload("res://addons/meowody_atlas/packer.gd")
const Profile = preload("res://addons/meowody_atlas/atlas_platform_settings.gd")
var failures := 0
var latest := {}


func check(condition: bool, message: String) -> void:
	print("[AtlasTest] ", "PASS " if condition else "FAIL ", message)
	if not condition:
		failures += 1


func png(path: String, size: Vector2i, color: Color, rect := Rect2i()) -> void:
	var image := Image.create(size.x, size.y, false, Image.FORMAT_RGBA8)
	image.fill(color if rect.size == Vector2i.ZERO else Color.TRANSPARENT)
	if rect.size != Vector2i.ZERO:
		image.fill_rect(rect, color)
	image.save_png(path)


func _finished(success: bool, message: String) -> void:
	latest = {"ok": success, "message": message}


func _wait_for_change(plugin: EditorPlugin, atlas: Resource, previous: String) -> Dictionary:
	EditorInterface.get_resource_filesystem().scan()
	var deadline := Time.get_ticks_msec() + 15000
	while Time.get_ticks_msec() < deadline:
		await plugin.get_tree().process_frame
		var manifest := Builder.read_manifest(Builder.manifest_path(atlas))
		if not plugin.busy and manifest.get("fingerprint", "") != previous:
			return manifest
	return Builder.read_manifest(Builder.manifest_path(atlas))


func run_fresh(plugin: EditorPlugin) -> void:
	await plugin.get_tree().create_timer(2.0).timeout
	var atlas: Resource = load("res://CommonAtlas.tres")
	var sprite: Texture2D = load("res://art/icons/red.png")
	check(sprite is AtlasTexture and sprite.get_size() == Vector2(20, 10), "fresh checkout imports atlas PNG without .godot cache")
	check(plugin.prepare_export().is_empty(), "export rebuilds missing platform cache, including manual atlases")
	var manifest_file := Builder.manifest_path(atlas)
	var manifest := Builder.read_manifest(manifest_file)
	DirAccess.remove_absolute("res://CommonAtlas.tres")
	EditorInterface.get_resource_filesystem().scan()
	var deadline := Time.get_ticks_msec() + 15000
	while Time.get_ticks_msec() < deadline:
		await plugin.get_tree().process_frame
		if not FileAccess.file_exists(manifest_file) and not plugin.refreshing:
			break
	check(not FileAccess.file_exists(manifest_file) and not FileAccess.file_exists(manifest.pages[0]), "deleting atlas asset removes its owned generated files")
	check(ResourceLoader.load("res://art/icons/red.png", "", ResourceLoader.CACHE_MODE_REPLACE) is CompressedTexture2D, "deleting atlas restores PNG importer")
	print("[AtlasTest] failures=", failures)
	plugin.get_tree().quit(0 if failures == 0 else 1)


func run(plugin: EditorPlugin) -> void:
	await plugin.get_tree().create_timer(1.0).timeout
	plugin.operation_finished.connect(_finished)
	check(not plugin.busy, "plugin starts idle")
	DirAccess.make_dir_recursive_absolute("res://art/icons")
	png("res://art/icons/red.png", Vector2i(20, 10), Color.RED)
	png("res://art/icons/green.png", Vector2i(24, 24), Color.GREEN, Rect2i(5, 7, 9, 8))
	png("res://art/icons/empty.png", Vector2i(8, 8), Color.TRANSPARENT)
	png("res://art/standalone.png", Vector2i(10, 10), Color.BLUE)
	await plugin._refresh_imports(PackedStringArray())
	var before := TextureRect.new()
	before.name = "BeforeAtlas"
	before.texture = load("res://art/icons/red.png")
	var before_scene := PackedScene.new()
	before_scene.pack(before)
	ResourceSaver.save(before_scene, "res://before.tscn")
	before.free()
	EditorInterface.open_scene_from_path("res://before.tscn")
	await plugin.get_tree().process_frame
	EditorInterface.get_edited_scene_root().modulate = Color(0.5, 0.6, 0.7)
	var atlas := Atlas.new()
	atlas.packables = PackedStringArray(["res://art/icons"])
	atlas.max_size = 64
	atlas.trim_transparent = true
	atlas.padding = 1
	atlas.extrude = 1
	ResourceSaver.save(atlas, "res://CommonAtlas.tres")
	atlas = load("res://CommonAtlas.tres")
	await plugin.request(atlas, true)
	check(latest.get("ok", false) and not DirAccess.dir_exists_absolute(Builder.OUTPUT), "preview writes no atlas or import files")
	atlas.android.override_enabled = true
	atlas.android.compression = Profile.Compression.ETC2_RGBA8
	atlas.ios.override_enabled = true
	atlas.ios.compression = Profile.Compression.ASTC_4X4
	atlas.desktop.override_enabled = true
	atlas.desktop.compression = Profile.Compression.BC7
	atlas.web.override_enabled = true
	atlas.web.compression = Profile.Compression.UNCOMPRESSED
	await plugin.request(atlas, false)
	check(latest.get("ok", false), "pack and real PNG reimport complete")
	check(EditorInterface.get_edited_scene_root().texture is AtlasTexture, "existing open scene adopts the atlas without reassigning its texture")
	check(EditorInterface.get_edited_scene_root().modulate.is_equal_approx(Color(0.5, 0.6, 0.7)), "import transition preserves unsaved scene edits")
	var manifest := Builder.read_manifest(Builder.manifest_path(atlas))
	if manifest.is_empty():
		plugin.get_tree().quit(1)
		return
	var runtime_atlas: Resource = load(manifest.catalog)
	check(runtime_atlas.get_class() == "SpriteAtlas" and runtime_atlas.get_script() == null, "catalog is a native C++ SpriteAtlas without a runtime script")
	check(runtime_atlas.get_sprite("red") == runtime_atlas.get_sprite("art/icons/red.png"), "C++ lookup resolves a unique short name and a qualified path")
	check(not runtime_atlas.has_sprite("missing") and runtime_atlas.get_sprite("missing") == null, "missing sprite returns an empty reference")
	check(runtime_atlas.get_sprite_names().size() == 3, "native catalog enumerates canonical sprite names")
	var red: Texture2D = load("res://art/icons/red.png")
	var green: Texture2D = load("res://art/icons/green.png")
	check(red is AtlasTexture and green is AtlasTexture, "original PNGs load as AtlasTexture")
	check(red.get_size() == Vector2(20, 10) and green.get_size() == Vector2(24, 24), "trim preserves original UI size")
	check(load("res://art/standalone.png") is CompressedTexture2D, "unrelated PNG keeps built-in importer")
	check(red.atlas == green.atlas, "sprites share a single atlas texture")
	var region: Rect2 = red.region
	var pixels: Image = red.atlas.get_image()
	check(pixels.get_pixelv(Vector2i(region.position)) == Color.RED and pixels.get_pixelv(Vector2i(region.position) - Vector2i.ONE) == Color.RED, "pixels and extruded corners are correct")
	check(green.margin == Rect2(5, 7, 15, 16), "trim offset and total transparent margin are correct")
	for target in {"android": Image.FORMAT_ETC2_RGBA8, "ios": Image.FORMAT_ASTC_4x4, "desktop": Image.FORMAT_BPTC_RGBA, "web": Image.FORMAT_RGBA8}:
		var encoded := FileAccess.open(manifest.variants[target][0], FileAccess.READ)
		encoded.seek(48)
		check(encoded.get_32() == {"android": Image.FORMAT_ETC2_RGBA8, "ios": Image.FORMAT_ASTC_4x4, "desktop": Image.FORMAT_BPTC_RGBA, "web": Image.FORMAT_RGBA8}[target], target + " page contains the requested encoded format")
	var scene := TextureRect.new()
	scene.name = "AtlasSample"
	scene.texture = red
	var packed := PackedScene.new()
	packed.pack(scene)
	ResourceSaver.save(packed, "res://sample.tscn")
	scene.free()
	check(FileAccess.get_file_as_string("res://sample.tscn").contains("res://art/icons/red.png"), "scene stores original PNG path instead of atlas coordinates")
	var uid := ResourceLoader.get_resource_uid("res://art/icons/red.png")
	var sprite_uid := ResourceLoader.get_resource_uid(manifest.sprites["res://art/icons/red.png"].resource)
	var generated: String = manifest.sprites["res://art/icons/green.png"].resource
	# A v1 manifest can contain platform resources in the visible output directory.
	var legacy_page := Builder.manifest_path(atlas).get_base_dir().path_join("platforms/legacy/page_0.res")
	DirAccess.make_dir_recursive_absolute(legacy_page.get_base_dir())
	var legacy_texture := ImageTexture.create_from_image(Image.create(4, 4, false, Image.FORMAT_RGBA8))
	ResourceSaver.save(legacy_texture, legacy_page)
	manifest.files.append(legacy_page)
	manifest.version = 1
	var legacy_manifest := FileAccess.open(Builder.manifest_path(atlas), FileAccess.WRITE)
	legacy_manifest.store_string(JSON.stringify(manifest))
	legacy_manifest.close()
	await plugin.request(atlas, false)
	check(latest.ok and not FileAccess.file_exists(legacy_page), "successful migration removes old visible platform resources")
	check(ResourceLoader.get_resource_uid(manifest.sprites["res://art/icons/red.png"].resource) == sprite_uid, "migration preserves existing sprite resource UID")
	manifest = Builder.read_manifest(Builder.manifest_path(atlas))
	png("res://art/icons/blue.png", Vector2i(40, 40), Color.BLUE)
	manifest = await _wait_for_change(plugin, atlas, manifest.fingerprint)
	check(manifest.sprites.has("res://art/icons/blue.png") and load("res://art/icons/blue.png") is AtlasTexture, "adding a PNG automatically repacks and redirects its import")
	check(ResourceLoader.get_resource_uid("res://art/icons/red.png") == uid, "source UID survives repacking")
	check(ResourceLoader.get_resource_uid(manifest.sprites["res://art/icons/red.png"].resource) == sprite_uid, "generated sprite UID survives repacking")
	var instance: TextureRect = (ResourceLoader.load("res://sample.tscn", "", ResourceLoader.CACHE_MODE_IGNORE) as PackedScene).instantiate()
	check(instance.texture is AtlasTexture and instance.texture.get_size() == Vector2(20, 10), "saved scene resolves the repacked atlas")
	instance.free()
	png("res://art/icons/red.png", Vector2i(20, 10), Color.YELLOW)
	manifest = await _wait_for_change(plugin, atlas, manifest.fingerprint)
	red = ResourceLoader.load("res://art/icons/red.png", "", ResourceLoader.CACHE_MODE_REPLACE)
	var fresh_page: Texture2D = ResourceLoader.load(manifest.pages[0], "", ResourceLoader.CACHE_MODE_IGNORE_DEEP)
	# Dummy rendering does not implement texture_replace; check persisted pixels headlessly.
	var updated_image: Image = fresh_page.get_image() if DisplayServer.get_name() == "headless" else red.atlas.get_image()
	check(updated_image.get_pixelv(Vector2i(red.region.position)) == Color.YELLOW, "modifying PNG pixels updates the atlas")
	DirAccess.remove_absolute("res://art/icons/green.png")
	manifest = await _wait_for_change(plugin, atlas, manifest.fingerprint)
	check(not manifest.sprites.has("res://art/icons/green.png") and not FileAccess.file_exists(generated), "deleting PNG removes its generated tres and repacks")
	var old_blue: String = manifest.sprites["res://art/icons/blue.png"].resource
	DirAccess.rename_absolute("res://art/icons/blue.png", "res://art/icons/renamed.png")
	manifest = await _wait_for_change(plugin, atlas, manifest.fingerprint)
	check(manifest.sprites.has("res://art/icons/renamed.png") and not FileAccess.file_exists(old_blue), "renaming PNG removes old derived resource and imports the new name")
	# An oversized source must fail without altering the last good outputs.
	atlas.auto_repack = false
	var good_hash := FileAccess.get_md5(manifest.pages[0])
	png("res://art/icons/huge.png", Vector2i(100, 100), Color.WHITE)
	await plugin.request(atlas, false)
	check(not latest.ok and FileAccess.get_md5(manifest.pages[0]) == good_hash, "failed packing preserves previous atlas pages")
	plugin.error_dialog.hide()
	DirAccess.remove_absolute("res://art/icons/huge.png")
	# Algorithm checks cover all three size modes and multi-page packing.
	var settings: Dictionary = atlas.settings()
	settings.max_size = 32
	settings.packables = ["res://art/icons/red.png", "res://art/icons/empty.png"]
	settings.trim = false
	for mode in [0, 1, 2]:
		settings.size_mode = mode
		var first := Packer.build(settings)
		var second := Packer.build(settings)
		check(first.sprites == second.sprites, "deterministic layout mode " + str(mode))
		for image in first.pages:
			check(image.get_width() <= 32 and image.get_height() <= 32, "page respects Max Size")
			if mode > 0:
				check(image.get_width() == nearest_po2(image.get_width()) and image.get_height() == nearest_po2(image.get_height()), "POT mode dimensions")
			if mode == 2:
				check(image.get_width() == image.get_height(), "square POT dimensions")
	# Restore source import settings when a PNG is removed from the atlas configuration.
	atlas.packables = PackedStringArray(["res://art/icons/red.png"])
	await plugin.request(atlas, false)
	check(latest.ok and load("res://art/icons/renamed.png") is CompressedTexture2D, "removing a packable restores the ordinary PNG importer")
	check(not FileAccess.file_exists(manifest.sprites["res://art/icons/renamed.png"].resource), "removed packable has no stale generated resource")
	# Explicit packable overlap is rejected without changing either working atlas.
	var conflict := Atlas.new()
	conflict.packables = atlas.packables
	ResourceSaver.save(conflict, "res://ConflictAtlas.tres")
	conflict = load("res://ConflictAtlas.tres")
	await plugin.request(conflict, false)
	check(not latest.ok and latest.message.contains("already belongs"), "overlapping atlases report the owning asset")
	plugin.error_dialog.hide()
	DirAccess.remove_absolute("res://ConflictAtlas.tres")
	await plugin._refresh_imports(PackedStringArray())
	# Force multi-page output; no overlaps and every PNG must appear exactly once.
	settings.packables = ["res://art/icons/red.png", "res://art/icons/renamed.png"]
	settings.max_size = 64
	settings.padding = 6
	settings.extrude = 1
	settings.size_mode = 0
	var multi := Packer.build(settings)
	check(multi.error.is_empty() and multi.pages.size() == 2 and multi.sprites.size() == 2, "overflow produces independently cropped pages")
	await _test_formats(plugin)
	# Export pumps the editor loop: start outside a SceneTreeTimer callback.
	await plugin.get_tree().process_frame
	ResourceSaver.save(load(Builder.read_manifest(Builder.manifest_path(atlas)).catalog).duplicate(), "res://runtime_atlas.tres")
	# Actual export callbacks, not just testing the platform-selection helper.
	var platform := EditorExportPlatformWindows.new()
	var preset := platform.create_preset()
	var export_error := platform.export_pack(preset, false, ProjectSettings.globalize_path("res://atlas-test.pck"))
	check(export_error == OK, "real Windows PCK export succeeds")
	var android_platform := EditorExportPlatformAndroid.new()
	var android_preset := android_platform.create_preset()
	check(android_platform.export_pack(android_preset, false, ProjectSettings.globalize_path("res://atlas-android.pck")) == OK, "real Android PCK export succeeds")
	EditorInterface.edit_resource(atlas)
	await plugin.get_tree().create_timer(1.0).timeout
	if DisplayServer.get_name() != "headless":
		await plugin.get_tree().process_frame
		plugin.get_viewport().get_texture().get_image().save_png("res://atlas-inspector.png")
		EditorInterface.edit_resource(load(Builder.read_manifest(Builder.manifest_path(atlas)).catalog))
		await plugin.get_tree().create_timer(0.5).timeout
		await plugin.get_tree().process_frame
		plugin.get_viewport().get_texture().get_image().save_png("res://atlas-native-inspector.png")
	plugin._disable_plugin()
	check(ResourceLoader.load("res://art/icons/red.png", "", ResourceLoader.CACHE_MODE_REPLACE) is CompressedTexture2D, "disabling plugin restores original PNG import settings")
	check(FileAccess.file_exists(Builder.manifest_path(atlas)), "disabling preserves generated atlas assets")
	# Restore atlas mode for the separate fresh-import test.
	plugin.stopping = false
	atlas.auto_repack = true
	await plugin.request(atlas, false)
	check(latest.ok, "atlas can be applied again after disable")
	print("[AtlasTest] failures=", failures)
	await plugin.get_tree().create_timer(0.5).timeout
	plugin.get_tree().quit(0 if failures == 0 else 1)


func _test_formats(plugin: EditorPlugin) -> void:
	DirAccess.make_dir_recursive_absolute("res://formats")
	var image := Image.create(12, 10, false, Image.FORMAT_RGBA8)
	image.fill(Color.CORNFLOWER_BLUE)
	image.save_png("res://formats/icon.png")
	image.save_jpg("res://formats/icon.jpg")
	image.save_jpg("res://formats/photo.JPEG")
	image.save_webp("res://formats/web.webp")
	var svg := FileAccess.open("res://formats/vector.svg", FileAccess.WRITE)
	svg.store_string('<svg xmlns="http://www.w3.org/2000/svg" width="12" height="10"><rect width="12" height="10" fill="red"/></svg>')
	svg.close()
	await plugin._refresh_imports(PackedStringArray())
	var atlas := Atlas.new()
	atlas.packables = PackedStringArray(["res://formats"])
	atlas.auto_repack = false
	ResourceSaver.save(atlas, "res://FormatsAtlas.tres")
	atlas = load("res://FormatsAtlas.tres")
	await plugin.request(atlas, false)
	check(latest.ok, "PNG/JPG/JPEG/WebP/SVG pack together")
	var manifest := Builder.read_manifest(Builder.manifest_path(atlas))
	check(manifest.get("sprites", {}).size() == 5, "all common image inputs are collected")
	if not latest.ok:
		return
	check(manifest.sprites["res://formats/icon.png"].resource != manifest.sprites["res://formats/icon.jpg"].resource, "same basename with different extensions keeps distinct sprite resources")
	for source in manifest.sprites:
		check(load(source) is AtlasTexture, source + " imports as AtlasTexture")
	var catalog: Resource = load(manifest.catalog)
	check(not catalog.has_sprite("icon") and catalog.has_sprite("formats/icon.jpg"), "ambiguous short names require a qualified sprite path")
	var panel := preload("res://addons/meowody_atlas/catalog_panel.gd").new()
	panel.atlas = catalog
	plugin.add_child(panel)
	panel.search.text = "web.webp"
	await plugin.get_tree().process_frame
	check(panel.list.item_count == 1, "native atlas Inspector searches canonical sprite names")
	panel.queue_free()
	# Leave this atlas for export and fresh checkout tests too.
