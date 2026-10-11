@tool
extends EditorPlugin

const Inspector = preload("inspector.gd")
const PanelEditor = preload("panel.gd")
var inspector: EditorInspectorPlugin
var panel: Control
var choose: ConfirmationDialog
var choices: ItemList
var selected_callback: Callable
var source_path := ""

func _enter_tree() -> void:
	if not ClassDB.class_exists("MultiPatchSprite"):
		push_error("MultiPatch: run scons, then reload the project to load the native backend.")
		return
	panel = PanelEditor.new()
	panel.manager = self
	add_control_to_bottom_panel(panel, "MultiPatch")
	inspector = Inspector.new()
	inspector.manager = self
	add_inspector_plugin(inspector)
	choose = ConfirmationDialog.new()
	choose.title = "Choose MultiPatch Sprite"
	choose.min_size = Vector2i(460, 300)
	choices = ItemList.new()
	choices.custom_minimum_size = Vector2(440, 260)
	choose.add_child(choices)
	add_child(choose)
	choose.confirmed.connect(_choose_confirmed)
	choices.item_activated.connect(func(_index: int): _choose_confirmed(); choose.hide())
	EditorInterface.get_resource_filesystem().resources_reimported.connect(_reimported)
	if OS.get_cmdline_user_args().has("--test-multipatch"):
		call_deferred("_run_tests")

func _exit_tree() -> void:
	if inspector != null:
		remove_inspector_plugin(inspector)
		EditorInterface.get_resource_filesystem().resources_reimported.disconnect(_reimported)
	if panel != null:
		remove_control_from_bottom_panel(panel)
		panel.queue_free()
	if choose != null:
		choose.queue_free()

func _save_external_data() -> void:
	if panel != null and panel.dirty:
		panel.apply()

func _handles(object: Object) -> bool:
	return object is Resource and object.get_class() == "MultiPatchSprite"

func _edit(object: Object) -> void:
	if object != null and panel != null:
		open_sprite(object)

func open_sprite(sprite: Resource) -> void:
	panel.request_open(sprite.texture, sprite)
	make_bottom_panel_item_visible(panel)

func open_texture(texture: Texture2D) -> void:
	if texture.get_class() == "MultiPatchTexture":
		if texture.sprite != null:
			open_sprite(texture.sprite)
		return
	texture = original_texture(texture)
	var sprites := find_sprites(texture)
	panel.request_open(texture, sprites[0] if not sprites.is_empty() else null)
	make_bottom_panel_item_visible(panel)

func open_atlas(atlas: Resource) -> void:
	var values: Array[Resource] = []
	for name in atlas.get_sprite_names():
		values.append(atlas.get_sprite(name))
	_show_choices(values, atlas.get_sprite_names(), open_texture)

func original_texture(texture: Texture2D) -> Texture2D:
	# Generated AtlasTextures refer to the same source. Keep sidecars next to that source.
	var path := texture.resource_path
	if path.begins_with("res://generated/ui_atlases/"):
		var root := DirAccess.open("res://generated/ui_atlases")
		if root != null:
			for folder in root.get_directories():
				var manifest_path := "res://generated/ui_atlases/" + folder + "/manifest.json"
				if not FileAccess.file_exists(manifest_path):
					continue
				var manifest = JSON.parse_string(FileAccess.get_file_as_string(manifest_path))
				if manifest is Dictionary:
					for source in manifest.get("sprites", {}):
						if manifest.sprites[source].resource == path:
							return load(source)
	return texture

func _sprite_paths(directory: EditorFileSystemDirectory, result: PackedStringArray) -> void:
	for i in range(directory.get_file_count()):
		if directory.get_file_type(i) == "MultiPatchSprite":
			result.append(directory.get_file_path(i))
	for i in range(directory.get_subdir_count()):
		_sprite_paths(directory.get_subdir(i), result)

func find_sprites(texture: Texture2D) -> Array[Resource]:
	var result: Array[Resource] = []
	if texture == null:
		return result
	var paths := PackedStringArray()
	_sprite_paths(EditorInterface.get_resource_filesystem().get_filesystem(), paths)
	for path in paths:
		var sprite: Resource = load(path)
		if sprite != null and sprite.texture != null and sprite.texture.resource_path == texture.resource_path:
			result.append(sprite)
	return result

func pick_sprite(texture: Texture2D, callback: Callable) -> void:
	texture = original_texture(texture)
	var sprites := find_sprites(texture)
	if sprites.is_empty():
		open_texture(texture)
		panel.status.text = "Create and Apply a sprite first, then drop this image again."
	elif sprites.size() == 1:
		callback.call(sprites[0])
	else:
		var names := PackedStringArray()
		for sprite in sprites:
			names.append(sprite.resource_name if not sprite.resource_name.is_empty() else sprite.resource_path.get_file())
		_show_choices(sprites, names, callback)

func _show_choices(values: Array[Resource], names: PackedStringArray, callback: Callable) -> void:
	choices.clear()
	for i in range(values.size()):
		choices.add_item(names[i])
		choices.set_item_metadata(i, values[i])
	selected_callback = callback
	choose.get_ok_button().disabled = values.is_empty()
	if not values.is_empty():
		choices.select(0)
	choose.popup_centered()

func _choose_confirmed() -> void:
	if selected_callback.is_valid() and not choices.get_selected_items().is_empty():
		selected_callback.call(choices.get_item_metadata(choices.get_selected_items()[0]))

func _reimported(_paths: PackedStringArray) -> void:
	# Wait until Atlas's importer has finished changing the source's native resource type.
	_refresh_source.call_deferred()

func _refresh_source() -> void:
	# Cached sidecars outside open scenes can still hold the detached, old texture.
	var paths := PackedStringArray()
	_sprite_paths(EditorInterface.get_resource_filesystem().get_filesystem(), paths)
	for path in paths:
		var cached := ResourceLoader.get_cached_ref(path)
		if cached != null and cached.texture != null and cached.texture.resource_path.is_empty():
			var fresh := ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_IGNORE)
			if fresh != null:
				cached.texture = fresh.texture
	if panel == null or panel.draft == null or source_path.is_empty():
		return
	if panel.draft.texture != null and not panel.draft.texture.resource_path.is_empty():
		source_path = panel.draft.texture.resource_path
	if ResourceLoader.exists(source_path):
		var texture: Texture2D = load(source_path)
		panel.draft.texture = texture
		panel.canvas.queue_redraw()

func _run_tests() -> void:
	await load("res://tests/meowody_multipatch/editor_test.gd").new().run(self)
