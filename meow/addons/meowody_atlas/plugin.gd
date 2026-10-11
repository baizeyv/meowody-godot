@tool
extends EditorPlugin

signal operation_finished(success: bool, message: String)

const Atlas = preload("ui_sprite_atlas.gd")
const Builder = preload("builder.gd")
const Packer = preload("packer.gd")
const Inspector = preload("inspector.gd")
const Importer = preload("sprite_importer.gd")
const Exporter = preload("export_plugin.gd")
const SceneTextures = preload("scene_textures.gd")

var inspector: EditorInspectorPlugin
var importer: EditorImportPlugin
var exporter: EditorExportPlugin
var debounce: Timer
var busy := false
var worker: Thread
var error_dialog: AcceptDialog
var create_dialog: EditorFileDialog
var failures := {}
var pending := false
var stopping := false
var refreshing := false


func _enter_tree() -> void:
	importer = Importer.new()
	add_import_plugin(importer)
	inspector = Inspector.new()
	inspector.manager = self
	add_inspector_plugin(inspector)
	exporter = Exporter.new()
	exporter.manager = self
	add_export_plugin(exporter)
	debounce = Timer.new()
	debounce.one_shot = true
	debounce.wait_time = 0.5
	debounce.timeout.connect(_refresh_atlases)
	add_child(debounce)
	error_dialog = AcceptDialog.new()
	error_dialog.title = "UI Sprite Atlas"
	add_child(error_dialog)
	create_dialog = EditorFileDialog.new()
	create_dialog.file_mode = EditorFileDialog.FILE_MODE_SAVE_FILE
	create_dialog.access = EditorFileDialog.ACCESS_RESOURCES
	create_dialog.filters = PackedStringArray(["*.tres ; UI Sprite Atlas"])
	create_dialog.file_selected.connect(_create_atlas)
	add_child(create_dialog)
	add_tool_menu_item("Create UI Sprite Atlas...", _choose_atlas_path)
	add_tool_menu_item("Repack All UI Atlases", _repack_all)
	EditorInterface.get_resource_filesystem().filesystem_changed.connect(schedule_refresh)
	EditorInterface.get_resource_filesystem().resources_reimported.connect(_reimported)
	resource_saved.connect(_resource_saved)
	EditorInterface.get_inspector().property_edited.connect(_property_edited)
	schedule_refresh()
	if OS.get_cmdline_user_args().has("--test-ui-atlas"):
		call_deferred("_run_tests")
	elif OS.get_cmdline_user_args().has("--test-ui-atlas-fresh"):
		call_deferred("_run_fresh_tests")


func _exit_tree() -> void:
	stopping = true
	debounce.stop()
	if worker != null and worker.is_started():
		worker.wait_to_finish()
	EditorInterface.get_resource_filesystem().filesystem_changed.disconnect(schedule_refresh)
	EditorInterface.get_resource_filesystem().resources_reimported.disconnect(_reimported)
	EditorInterface.get_inspector().property_edited.disconnect(_property_edited)
	remove_tool_menu_item("Create UI Sprite Atlas...")
	remove_tool_menu_item("Repack All UI Atlases")
	remove_export_plugin(exporter)
	remove_inspector_plugin(inspector)
	remove_import_plugin(importer)
	debounce.free()
	error_dialog.free()
	create_dialog.free()


func _disable_plugin() -> void:
	# Godot calls this on explicit disable, not on normal editor shutdown.
	stopping = true
	debounce.stop()
	var paths := PackedStringArray()
	for manifest in Builder.manifests():
		var restored := Builder.remove_atlas(manifest, true)
		if restored.error.is_empty():
			paths.append_array(restored.reimport)
		else:
			push_error(restored.error)
	if not paths.is_empty():
		var detached := SceneTextures.detach_type_changes(paths)
		EditorInterface.get_resource_filesystem().reimport_files(paths)
		SceneTextures.reconnect(detached)


func _choose_atlas_path() -> void:
	create_dialog.current_file = "NewAtlas.tres"
	create_dialog.popup_centered_ratio(0.65)


func _create_atlas(path: String) -> void:
	var atlas := Atlas.new()
	if ResourceSaver.save(atlas, path) == OK:
		EditorInterface.get_resource_filesystem().update_file(path)
		EditorInterface.edit_resource(load(path))


func _resource_saved(resource: Resource) -> void:
	if resource is Atlas:
		schedule_refresh()


func _property_edited(_property: StringName) -> void:
	var edited := EditorInterface.get_inspector().get_edited_object()
	if edited is Atlas or edited is UIAtlasPlatformSettings:
		schedule_refresh()


func _reimported(_paths: PackedStringArray) -> void:
	schedule_refresh()


func schedule_refresh() -> void:
	if stopping:
		return
	pending = true
	debounce.start()


func _assets(directory: String = "res://") -> PackedStringArray:
	var paths := PackedStringArray()
	var dir := DirAccess.open(directory)
	if dir == null:
		return paths
	for file in dir.get_files():
		if file.get_extension() == "tres":
			var path := directory.path_join(file)
			var stream := FileAccess.open(path, FileAccess.READ)
			if stream != null and stream.get_line().contains('script_class="UISpriteAtlas"'):
				paths.append(path)
	for folder in dir.get_directories():
		if folder.begins_with(".") or folder in ["addons", "generated"]:
			continue
		paths.append_array(_assets(directory.path_join(folder)))
	return paths


func _refresh_atlases() -> void:
	if refreshing or busy or EditorInterface.get_resource_filesystem().is_scanning() or EditorInterface.get_resource_filesystem().is_importing():
		schedule_refresh()
		return
	refreshing = true
	pending = false
	var assets := _assets()
	# Clean up a deleted atlas configuration, but preserve outputs when its asset moved.
	var active_ids := {}
	for path in assets:
		var atlas: Resource = load(path)
		if atlas != null:
			active_ids[atlas.atlas_id] = path
	for path in Builder.manifests():
		var manifest := Builder.read_manifest(path)
		if not FileAccess.file_exists(manifest.asset) and not active_ids.has(path.get_base_dir().get_file()):
			var removed := Builder.remove_atlas(path)
			if removed.error.is_empty():
				await _refresh_imports(removed.reimport)
			else:
				_report(false, removed.error)
	for path in assets:
		var atlas: Resource = load(path)
		if atlas == null or not atlas.auto_repack:
			continue
		var manifest := Builder.read_manifest(Builder.manifest_path(atlas))
		# Preview stays non-destructive; watching starts after the first explicit Pack.
		if manifest.is_empty():
			continue
		var settings: Dictionary = atlas.settings()
		var fingerprint := Packer.fingerprint(settings, Packer.collect(settings))
		if (not Builder.cache_ready(manifest) or manifest.fingerprint != fingerprint or manifest.asset != path or not _imports_match(manifest)) and failures.get(path, "") != fingerprint:
			failures[path] = fingerprint
			await request(atlas, false)
			if stopping:
				return
	refreshing = false


func _imports_match(manifest: Dictionary) -> bool:
	for path in manifest.sprites:
		var config := ConfigFile.new()
		if config.load(path + ".import") != OK or config.get_value("remap", "importer", "") != Builder.IMPORTER:
			return false
	return true


func request(atlas: Resource, preview_only: bool, panel: Control = null) -> void:
	if busy:
		if is_instance_valid(panel):
			panel.set_busy(false, "Another atlas is being processed. Try again shortly.")
		return
	busy = true
	if is_instance_valid(panel):
		panel.set_busy(true, "Packing layout..." if preview_only else "Packing and encoding platform textures...")
	if not preview_only:
		var error := Builder.prepare(atlas)
		if not error.is_empty():
			_complete(false, error, panel)
			return
	var settings: Dictionary = atlas.settings()
	worker = Thread.new()
	var started := worker.start(Builder.build.bind(settings, not preview_only))
	if started != OK:
		_complete(false, "Cannot start atlas worker: " + error_string(started), panel)
		return
	while worker.is_alive():
		await get_tree().process_frame
		if stopping:
			return
	var result: Dictionary = worker.wait_to_finish()
	worker = null
	if not result.error.is_empty():
		_complete(false, result.error, panel)
		return
	if not preview_only:
		# Do not apply stale pixels if a file changed while the worker was packing.
		var current_settings: Dictionary = atlas.settings()
		if result.fingerprint != Packer.fingerprint(current_settings, Packer.collect(current_settings)):
			_complete(false, "Atlas inputs changed while packing. Pack again.", panel)
			return
		var committed := Builder.commit(atlas, result)
		if not committed.error.is_empty():
			_complete(false, committed.error, panel)
			return
		if is_instance_valid(panel):
			panel.set_busy(true, "Refreshing PNG imports and scene textures...")
		await _refresh_imports(committed.reimport)
		for source in committed.manifest.sprites:
			var texture = ResourceLoader.load(source, "", ResourceLoader.CACHE_MODE_REPLACE)
			if not texture is AtlasTexture:
				_complete(false, "PNG import did not resolve to AtlasTexture: " + source, panel)
				return
		# A successful removal may unblock another atlas waiting to own the same PNG.
		failures.clear()
	if is_instance_valid(panel):
		panel.show_result(result)
	_complete(true, "%s: %d sprites in %d page(s)." % ["Preview ready" if preview_only else "Atlas applied", result.sprites.size(), result.pages.size()], panel)


func _refresh_imports(paths: PackedStringArray) -> void:
	var filesystem := EditorInterface.get_resource_filesystem()
	var detached := SceneTextures.detach_type_changes(paths)
	filesystem.scan()
	await filesystem.filesystem_changed
	# This signal may be emitted from inside Godot's import progress dialog.
	# Leave that call stack before starting another reimport.
	await get_tree().process_frame
	while filesystem.is_scanning() or filesystem.is_importing():
		await get_tree().process_frame
	if not paths.is_empty():
		filesystem.reimport_files(paths)
	while filesystem.is_importing():
		await get_tree().process_frame
	await get_tree().process_frame
	SceneTextures.reconnect(detached)


func _complete(success: bool, message: String, panel: Control) -> void:
	busy = false
	if is_instance_valid(panel):
		panel.set_busy(false, message)
	_report(success, message)
	if pending:
		schedule_refresh()


func _report(success: bool, message: String) -> void:
	if success:
		print("[UIAtlas] ", message)
	else:
		push_error("[UIAtlas] " + message)
		error_dialog.dialog_text = message
		error_dialog.popup_centered(Vector2i(600, 180))
	operation_finished.emit(success, message)


func _repack_all() -> void:
	for path in _assets():
		await request(load(path), false)


# Export is synchronous in Godot. Only stale atlases are rebuilt here.
func prepare_export() -> String:
	if busy:
		return "An atlas is still packing. Wait for it to finish before exporting."
	for path in _assets():
		var atlas: Resource = load(path)
		var settings: Dictionary = atlas.settings()
		var manifest := Builder.read_manifest(Builder.manifest_path(atlas))
		if Builder.cache_ready(manifest) and manifest.get("fingerprint", "") == Packer.fingerprint(settings, Packer.collect(settings)) and manifest.get("asset", "") == path:
			continue
		var error := Builder.prepare(atlas)
		if not error.is_empty():
			return error
		var result := Builder.build(settings)
		if not result.error.is_empty():
			return result.error
		var committed := Builder.commit(atlas, result)
		if not committed.error.is_empty():
			return committed.error
		for output in committed.manifest.files:
			if not str(output).begins_with("res://.godot/") and not str(output).contains("/data/"):
				EditorInterface.get_resource_filesystem().update_file(output)
		if not committed.reimport.is_empty():
			var detached := SceneTextures.detach_type_changes(committed.reimport)
			EditorInterface.get_resource_filesystem().reimport_files(committed.reimport)
			SceneTextures.reconnect(detached)
	return ""


func _run_tests() -> void:
	await load("res://tests/meowody_atlas/editor_test.gd").new().run(self)


func _run_fresh_tests() -> void:
	await load("res://tests/meowody_atlas/editor_test.gd").new().run_fresh(self)
