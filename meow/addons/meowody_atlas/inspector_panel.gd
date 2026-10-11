@tool
extends VBoxContainer

const DropList = preload("packables_list.gd")
const Preview = preload("preview.gd")
const Builder = preload("builder.gd")
var atlas: Resource
var manager: EditorPlugin
var list: ItemList
var preview: Control
var status: Label
var statistics: Label
var page_picker: OptionButton
var buttons: Array[Button] = []
var result := {}
var file_dialog: EditorFileDialog
var platform_picker: OptionButton
var platform_options: VBoxContainer
const TARGETS := ["default_settings", "android", "ios", "desktop", "web"]
const FORMATS := ["Lossless", "Uncompressed", "ETC2 RGBA8", "ASTC 4×4", "ASTC 8×8", "BC7"]


func _ready() -> void:
	add_theme_constant_override("separation", 10)
	var title := Label.new()
	title.text = "UI Sprite Atlas"
	title.add_theme_font_size_override("font_size", int(19 * EditorInterface.get_editor_scale()))
	add_child(title)
	var hint := Label.new()
	hint.text = "Drop images or folders below. Pack generates atlas.tres (native SpriteAtlas). Assign it to your C++ atlas property, then call get_sprite(name)."
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	add_child(hint)
	list = DropList.new()
	list.custom_minimum_size.y = 130 * EditorInterface.get_editor_scale()
	list.select_mode = ItemList.SELECT_MULTI
	list.paths_dropped.connect(_add_paths)
	add_child(list)
	var row := HBoxContainer.new()
	add_child(row)
	_button(row, "+ Image", _choose_files)
	_button(row, "+ Folder", _choose_folder)
	_button(row, "Remove", _remove_selected)
	file_dialog = EditorFileDialog.new()
	file_dialog.access = EditorFileDialog.ACCESS_RESOURCES
	file_dialog.files_selected.connect(_add_paths)
	file_dialog.dir_selected.connect(func(path: String) -> void: _add_paths(PackedStringArray([path])))
	add_child(file_dialog)
	platform_picker = OptionButton.new()
	for target in ["Default", "Android", "iOS", "Desktop", "Web"]:
		platform_picker.add_item("Platform: " + target)
	platform_picker.item_selected.connect(func(_index: int) -> void: _refresh_profile())
	add_child(platform_picker)
	platform_options = VBoxContainer.new()
	add_child(platform_options)
	_refresh_profile()
	row = HBoxContainer.new()
	add_child(row)
	_button(row, "Pack Preview", func() -> void: manager.request(atlas, true, self))
	_button(row, "Pack & Apply", func() -> void: manager.request(atlas, false, self))
	_button(self, "Show Runtime Atlas", _show_runtime_atlas)
	status = Label.new()
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	status.text = "Preview does not change files. Pack applies the atlas and Image imports."
	add_child(status)
	page_picker = OptionButton.new()
	page_picker.item_selected.connect(_show_page)
	add_child(page_picker)
	preview = Preview.new()
	add_child(preview)
	statistics = Label.new()
	statistics.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	add_child(statistics)
	var outlines := CheckBox.new()
	outlines.text = "Show sprite boundaries"
	outlines.button_pressed = true
	outlines.toggled.connect(func(value: bool) -> void:
		preview.show_regions = value
		preview.queue_redraw())
	add_child(outlines)
	var note := Label.new()
	note.text = "Packing options below · Rotation off for UI compatibility"
	note.tooltip_text = "Filtering/repeat are CanvasItem settings in Godot 4. UI colors follow Godot's normal color pipeline. ASTC supports 4×4 and 8×8; encoder quality is controlled by Godot. Mipmaps may bleed at low levels; increase padding or keep them disabled for UI."
	note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	note.modulate.a = 0.7
	add_child(note)
	atlas.changed.connect(_refresh)
	atlas.changed.connect(func() -> void: call_deferred("_refresh_profile"))
	_refresh()
	var manifest := Builder.read_manifest(Builder.manifest_path(atlas))
	if not manifest.is_empty():
		result = {"pages": [], "sprites": manifest.sprites, "area": 0, "used_area": 0}
		for path in manifest.pages:
			var image := Builder.editor_page_image(path)
			if image != null:
				result.pages.append(image)
		for entry in manifest.sprites.values():
			result.used_area += entry.region[2] * entry.region[3]
		show_result(result)


func _button(row: Control, text: String, action: Callable) -> void:
	var button := Button.new()
	button.text = text
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.pressed.connect(action)
	row.add_child(button)
	buttons.append(button)


func _refresh_profile() -> void:
	for child in platform_options.get_children():
		platform_options.remove_child(child)
		child.queue_free()
	var index := platform_picker.selected
	var profile: Resource = atlas.get(TARGETS[index])
	if profile == null:
		profile = preload("atlas_platform_settings.gd").new()
		atlas.set(TARGETS[index], profile)
	if index > 0:
		var enabled := CheckBox.new()
		enabled.text = "Override Default"
		enabled.button_pressed = profile.override_enabled
		enabled.toggled.connect(func(value: bool) -> void: _change_profile(profile, "override_enabled", value))
		platform_options.add_child(enabled)
	var effective: Resource = profile if index == 0 or profile.override_enabled else atlas.default_settings
	if effective == null:
		effective = preload("atlas_platform_settings.gd").new()
	var compression := OptionButton.new()
	var formats: Array = [[0, 1], [0, 1, 2, 3, 4], [0, 1, 2, 3, 4], [0, 1, 5], [0, 1]][index]
	for format in formats:
		compression.add_item(FORMATS[format], format)
	compression.select(formats.find(effective.compression))
	compression.disabled = index > 0 and not profile.override_enabled
	compression.item_selected.connect(func(item: int) -> void: _change_profile(profile, "compression", compression.get_item_id(item)))
	platform_options.add_child(compression)
	var mipmaps := CheckBox.new()
	mipmaps.text = "Generate mipmaps"
	mipmaps.button_pressed = effective.mipmaps
	mipmaps.disabled = compression.disabled
	mipmaps.toggled.connect(func(value: bool) -> void: _change_profile(profile, "mipmaps", value))
	platform_options.add_child(mipmaps)
	var resolved := Label.new()
	resolved.text = "Export: " + FORMATS[effective.compression] + (" (inherited)" if compression.disabled else "")
	platform_options.add_child(resolved)


func _change_profile(profile: Resource, property: String, value: Variant) -> void:
	var undo := manager.get_undo_redo()
	undo.create_action("Change atlas compression", UndoRedo.MERGE_DISABLE, atlas)
	undo.add_do_property(profile, property, value)
	undo.add_undo_property(profile, property, profile.get(property))
	undo.add_do_method(atlas, "emit_changed")
	undo.add_undo_method(atlas, "emit_changed")
	undo.commit_action()
	manager.schedule_refresh()


func _refresh() -> void:
	list.clear()
	for path in atlas.packables:
		var icon := "Folder" if DirAccess.dir_exists_absolute(path) else "ImageTexture"
		list.add_item(path, get_theme_icon(icon, "EditorIcons"))
		list.set_item_tooltip(list.item_count - 1, path)


func _choose_files() -> void:
	file_dialog.file_mode = EditorFileDialog.FILE_MODE_OPEN_FILES
	file_dialog.filters = PackedStringArray(["*.png,*.jpg,*.jpeg,*.webp,*.bmp,*.tga,*.svg,*.hdr,*.exr ; Images"])
	file_dialog.popup_centered_ratio(0.65)


func _choose_folder() -> void:
	file_dialog.file_mode = EditorFileDialog.FILE_MODE_OPEN_DIR
	file_dialog.filters = PackedStringArray()
	file_dialog.popup_centered_ratio(0.65)


func _add_paths(paths: PackedStringArray) -> void:
	var next: PackedStringArray = atlas.packables.duplicate()
	for path in paths:
		if not path in next:
			next.append(path)
	_change_packables(next)


func _remove_selected() -> void:
	var selected := list.get_selected_items()
	var next := PackedStringArray()
	for i in range(atlas.packables.size()):
		if not i in selected:
			next.append(atlas.packables[i])
	_change_packables(next)


func _change_packables(next: PackedStringArray) -> void:
	var undo := manager.get_undo_redo()
	undo.create_action("Change atlas packables", UndoRedo.MERGE_DISABLE, atlas)
	undo.add_do_property(atlas, "packables", next)
	undo.add_undo_property(atlas, "packables", atlas.packables)
	undo.add_do_method(atlas, "emit_changed")
	undo.add_undo_method(atlas, "emit_changed")
	undo.commit_action()
	manager.schedule_refresh()


func set_busy(busy: bool, text: String) -> void:
	for button in buttons:
		button.disabled = busy
	status.text = text


func show_result(value: Dictionary) -> void:
	result = value
	page_picker.clear()
	for i in range(result.pages.size()):
		page_picker.add_item("Page %d / %d" % [i + 1, result.pages.size()])
	if not result.pages.is_empty():
		_show_page(0)
	else:
		preview.texture = null
		preview.queue_redraw()
		statistics.text = "No images. Previous generated sprites and pages are removed when packed."


func _show_page(index: int) -> void:
	var image: Image = result.pages[index]
	preview.display(image, result.sprites, index)
	var area := 0
	for page in result.pages:
		area += page.get_width() * page.get_height()
	statistics.text = "%d × %d · %d sprites · %d page(s)\nContent occupancy: %.1f%% · Scroll to zoom" % [image.get_width(), image.get_height(),
		result.sprites.size(), result.pages.size(), 100.0 * result.used_area / maxi(1, area)]


func _show_runtime_atlas() -> void:
	var manifest := Builder.read_manifest(Builder.manifest_path(atlas))
	var path: String = manifest.get("catalog", "")
	if path.is_empty() or not ResourceLoader.exists(path):
		status.text = "Pack & Apply first to generate the runtime SpriteAtlas."
		return
	EditorInterface.select_file(path)
	EditorInterface.edit_resource(load(path))
