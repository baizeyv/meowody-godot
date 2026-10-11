@tool
extends VBoxContainer

const Canvas = preload("canvas.gd")
const FIELDS := Canvas.FIELDS
const MODES := ["Fixed", "Stretch", "Tile", "Tile Fit"]
var manager: EditorPlugin
var draft: Resource
var saved: Resource
var baseline := {}
var dirty := false
var undo := UndoRedo.new()
var updating := false
var canvas: Control
var preview: Control
var status: Label
var title: LineEdit
var sprite_picker: OptionButton
var siblings: Array[Resource] = []
var axis_picker: OptionButton
var segment_picker: OptionButton
var cut_position: SpinBox
var mode_picker: OptionButton
var weight: SpinBox
var region_values: Array[SpinBox] = []
var pending_open: Callable
var discard: ConfirmationDialog
var save_dialog: EditorFileDialog

func _ready() -> void:
	custom_minimum_size.y = 480 * EditorInterface.get_editor_scale()
	var toolbar := HBoxContainer.new()
	add_child(toolbar)
	_button(toolbar, "Apply / Save", apply)
	_button(toolbar, "Undo", func(): undo.undo())
	_button(toolbar, "Redo", func(): undo.redo())
	var mode := OptionButton.new()
	mode.add_item("Sprite Region")
	mode.add_item("Multi Patch")
	mode.select(1)
	mode.item_selected.connect(func(index: int): canvas.region_mode = index == 0; canvas.fit())
	toolbar.add_child(mode)
	_button(toolbar, "3 x 3", func(): preset(3))
	_button(toolbar, "4 x 4", func(): preset(4))
	_button(toolbar, "Fit", func(): canvas.fit())
	var layout := HSplitContainer.new()
	layout.size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_child(layout)
	var settings_scroll := ScrollContainer.new()
	settings_scroll.custom_minimum_size.x = 235 * EditorInterface.get_editor_scale()
	settings_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	layout.add_child(settings_scroll)
	var settings := VBoxContainer.new()
	settings.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	settings_scroll.add_child(settings)
	sprite_picker = OptionButton.new()
	settings.add_child(sprite_picker)
	sprite_picker.item_selected.connect(func(index: int): request_open(siblings[index].texture, siblings[index]))
	_button(settings, "New Sprite Region", func():
		if draft != null:
			request_open(draft.texture, null))
	title = LineEdit.new()
	title.placeholder_text = "Sprite name"
	settings.add_child(title)
	title.text_submitted.connect(func(_text: String): _change_name())
	title.focus_exited.connect(_change_name)
	_label(settings, "Region  X / Y / Width / Height")
	var grid := GridContainer.new()
	grid.columns = 2
	settings.add_child(grid)
	for i in range(4):
		var spin := _spin(grid, 0, 32768, 1, 0)
		region_values.append(spin)
		spin.value_changed.connect(func(_value: float): _region_changed())
	_button(settings, "Full Image", func():
		if draft != null:
			_change_region(Rect2i(Vector2i.ZERO, Vector2i(draft.texture.get_size()))))
	_button(settings, "Trim Transparent Region", _trim)
	axis_picker = OptionButton.new()
	axis_picker.add_item("Horizontal (X)")
	axis_picker.add_item("Vertical (Y)")
	settings.add_child(axis_picker)
	axis_picker.item_selected.connect(func(index: int): canvas.axis = index; _refresh_segments())
	segment_picker = OptionButton.new()
	settings.add_child(segment_picker)
	segment_picker.item_selected.connect(func(_index: int): _refresh_segment_values())
	_label(settings, "Segment end / cut position")
	cut_position = _spin(settings, 1, 32768, 1, 1)
	cut_position.value_changed.connect(_move_cut)
	var cut_buttons := HBoxContainer.new()
	settings.add_child(cut_buttons)
	_button(cut_buttons, "+ Cut", _add_cut)
	_button(cut_buttons, "Remove Cut", _remove_cut)
	mode_picker = OptionButton.new()
	for item in MODES:
		mode_picker.add_item(item)
	settings.add_child(mode_picker)
	mode_picker.item_selected.connect(_set_mode)
	_label(settings, "Flexible segment weight")
	weight = _spin(settings, 0.01, 1000, 0.1, 1)
	weight.value_changed.connect(_set_weight)
	var work := HSplitContainer.new()
	work.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	layout.add_child(work)
	canvas = Canvas.new()
	canvas.edited.connect(_record_edit)
	canvas.selected.connect(func(axis: int, segment: int):
		axis_picker.select(axis)
		_refresh_segments(segment))
	work.add_child(canvas)
	var preview_column := VBoxContainer.new()
	preview_column.custom_minimum_size.x = 260 * EditorInterface.get_editor_scale()
	work.add_child(preview_column)
	_label(preview_column, "Runtime Preview")
	var sizes := HBoxContainer.new()
	preview_column.add_child(sizes)
	var width := _spin(sizes, 1, 4096, 1, 240)
	var height := _spin(sizes, 1, 4096, 1, 140)
	var preview_scroll := ScrollContainer.new()
	preview_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	preview_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	preview_column.add_child(preview_scroll)
	var holder := Control.new()
	preview_scroll.add_child(holder)
	preview = ClassDB.instantiate("MultiPatchRect")
	holder.add_child(preview)
	var resize_preview := func(_value: float):
		holder.custom_minimum_size = Vector2(width.value, height.value)
		preview.size = holder.custom_minimum_size
	width.value_changed.connect(resize_preview)
	height.value_changed.connect(resize_preview)
	resize_preview.call(0)
	_label(preview_column, "Drag cuts · Double-click: add cut
Wheel: zoom · Middle mouse: pan
Delete: remove cut · F/S/T: mode
All cuts snap to source pixels.")
	status = _label(self, "Select a texture or SpriteAtlas sprite, then Open MultiPatch Editor.")
	discard = ConfirmationDialog.new()
	discard.title = "Unsaved MultiPatch edits"
	discard.dialog_text = "Discard the current edits? Cancel to keep editing or Apply them first."
	discard.confirmed.connect(func():
		if pending_open.is_valid():
			pending_open.call())
	add_child(discard)
	save_dialog = EditorFileDialog.new()
	save_dialog.file_mode = EditorFileDialog.FILE_MODE_SAVE_FILE
	save_dialog.access = EditorFileDialog.ACCESS_RESOURCES
	save_dialog.filters = PackedStringArray(["*.tres ; MultiPatch Sprite"])
	save_dialog.file_selected.connect(_save_to)
	add_child(save_dialog)

func _button(parent: Control, text: String, action: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.pressed.connect(action)
	parent.add_child(button)
	return button

func _label(parent: Control, text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	parent.add_child(label)
	return label

func _spin(parent: Control, low: float, high: float, step: float, value: float) -> SpinBox:
	var spin := SpinBox.new()
	spin.min_value = low
	spin.max_value = high
	spin.step = step
	spin.value = value
	spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(spin)
	return spin

func request_open(texture: Texture2D, sprite: Resource) -> void:
	if texture == null:
		status.text = "Assign the sprite's source Texture first."
		return
	pending_open = _open.bind(texture, sprite)
	if dirty:
		discard.popup_centered()
	else:
		pending_open.call()

func _open(texture: Texture2D, sprite: Resource) -> void:
	saved = sprite
	draft = sprite.duplicate() if sprite != null else ClassDB.instantiate("MultiPatchSprite")
	draft.texture = texture
	if sprite == null:
		draft.resource_name = texture.resource_path.get_file().get_basename() + "_patch"
	manager.source_path = texture.resource_path
	baseline = _state()
	undo.clear_history()
	canvas.sprite = draft
	preview.sprite = draft
	draft.changed.connect(_data_changed)
	siblings = manager.find_sprites(texture)
	sprite_picker.clear()
	for item in siblings:
		sprite_picker.add_item(item.resource_name)
	if saved != null and saved in siblings:
		sprite_picker.select(siblings.find(saved))
	sprite_picker.disabled = siblings.is_empty()
	_refresh()
	canvas.fit.call_deferred()

func _state() -> Dictionary:
	return canvas.snapshot() if canvas.sprite != null and canvas.sprite == draft else _resource_state(draft)

func _resource_state(resource: Resource) -> Dictionary:
	var result := {}
	for field in FIELDS:
		result[field] = resource.get(field)
	return result

func _restore(state: Dictionary) -> void:
	updating = true
	for field in FIELDS:
		draft.set(field, state[field])
	updating = false
	_refresh()

func _record_edit(before: Dictionary) -> void:
	var after := _state()
	if before == after:
		return
	undo.create_action("Edit MultiPatch")
	undo.add_do_method(_restore.bind(after))
	undo.add_undo_method(_restore.bind(before))
	undo.commit_action(false)
	_refresh()

func _data_changed() -> void:
	canvas.queue_redraw()
	if not updating:
		dirty = _state() != baseline
		var error: String = draft.get_validation_error()
		status.text = error if not error.is_empty() else ("Unsaved changes · " if dirty else "") + manager.source_path

func _refresh() -> void:
	updating = true
	title.text = draft.resource_name
	var region: Rect2i = draft.get_effective_region()
	for i in range(4):
		region_values[i].set_value_no_signal([region.position.x, region.position.y, region.size.x, region.size.y][i])
	updating = false
	_refresh_segments()
	_data_changed()

func _prefix() -> String:
	return "x_" if axis_picker.selected == 0 else "y_"

func _refresh_segments(index := -1) -> void:
	if draft == null:
		return
	if index < 0:
		index = maxi(0, segment_picker.selected)
	segment_picker.clear()
	var cuts: PackedInt32Array = draft.get(_prefix() + "cuts")
	var modes := _modes()
	var start := 0
	for i in range(cuts.size() + 1):
		var end: int = cuts[i] if i < cuts.size() else draft.get_effective_region().size[axis_picker.selected]
		segment_picker.add_item("%d: %d - %d  %s" % [i, start, end, MODES[clampi(modes[i], 0, 3)]])
		start = end
	segment_picker.select(mini(index, cuts.size()))
	_refresh_segment_values()

func _modes() -> PackedInt32Array:
	var cuts: PackedInt32Array = draft.get(_prefix() + "cuts")
	var modes: PackedInt32Array = draft.get(_prefix() + "modes")
	if modes.size() != cuts.size() + 1:
		modes.resize(cuts.size() + 1)
		modes.fill(1)
		if not cuts.is_empty():
			modes[0] = 0
			modes[-1] = 0
	return modes

func _refresh_segment_values() -> void:
	var index := segment_picker.selected
	var cuts: PackedInt32Array = draft.get(_prefix() + "cuts")
	cut_position.editable = index >= 0 and index < cuts.size()
	if cut_position.editable:
		cut_position.set_value_no_signal(cuts[index])
	mode_picker.select(clampi(_modes()[index], 0, 3))
	var weights: PackedFloat32Array = draft.get(_prefix() + "weights")
	weight.set_value_no_signal(weights[index] if index < weights.size() else 1.0)
	weight.editable = mode_picker.selected != 0
	canvas.axis = axis_picker.selected
	canvas.selected_cut = index if cut_position.editable else -1
	canvas.queue_redraw()

func _change_name() -> void:
	if draft == null or updating or title.text == draft.resource_name:
		return
	var before := _state()
	draft.resource_name = title.text.strip_edges()
	_record_edit(before)

func _region_changed() -> void:
	if draft != null and not updating:
		_change_region(Rect2i(int(region_values[0].value), int(region_values[1].value), int(region_values[2].value), int(region_values[3].value)))

func _change_region(region: Rect2i) -> void:
	var before := _state()
	draft.region = region
	_clear_axis("x_")
	_clear_axis("y_")
	_record_edit(before)
	canvas.fit()

func _clear_axis(prefix: String) -> void:
	draft.set(prefix + "cuts", PackedInt32Array())
	draft.set(prefix + "modes", PackedInt32Array())
	draft.set(prefix + "weights", PackedFloat32Array())

func preset(segments: int) -> void:
	if draft == null:
		return
	var before := _state()
	for direction in range(2):
		var extent: int = draft.get_effective_region().size[direction]
		var cuts := PackedInt32Array()
		for i in range(1, segments):
			var cut := roundi(float(extent) * i / segments)
			if cut > 0 and cut < extent and not cut in cuts:
				cuts.append(cut)
		var prefix := "x_" if direction == 0 else "y_"
		_clear_axis(prefix)
		draft.set(prefix + "cuts", cuts)
	_record_edit(before)

func _add_cut() -> void:
	if draft == null:
		return
	var cuts: PackedInt32Array = draft.get(_prefix() + "cuts")
	if cuts.size() >= 64:
		return
	var index := segment_picker.selected
	var start := cuts[index - 1] if index > 0 else 0
	var end: int = cuts[index] if index < cuts.size() else draft.get_effective_region().size[axis_picker.selected]
	if end - start < 2:
		return
	var before := _state()
	Canvas.edit_cut(draft, axis_picker.selected, index, (start + end) / 2)
	_record_edit(before)

func _remove_cut() -> void:
	if draft == null or canvas.selected_cut < 0:
		return
	var before := _state()
	Canvas.edit_cut(draft, axis_picker.selected, canvas.selected_cut)
	_record_edit(before)

func _move_cut(value: float) -> void:
	if draft == null or updating or not cut_position.editable:
		return
	var cuts: PackedInt32Array = draft.get(_prefix() + "cuts")
	var index := segment_picker.selected
	var lower := cuts[index - 1] + 1 if index > 0 else 1
	var upper: int = cuts[index + 1] - 1 if index + 1 < cuts.size() else draft.get_effective_region().size[axis_picker.selected] - 1
	var before := _state()
	cuts[index] = clampi(roundi(value), lower, upper)
	draft.set(_prefix() + "cuts", cuts)
	_record_edit(before)

func _set_mode(index: int) -> void:
	if draft == null or updating:
		return
	var before := _state()
	var modes := _modes()
	modes[segment_picker.selected] = index
	draft.set(_prefix() + "modes", modes)
	_record_edit(before)

func _set_weight(value: float) -> void:
	if draft == null or updating:
		return
	var before := _state()
	var weights: PackedFloat32Array = draft.get(_prefix() + "weights")
	if weights.size() != segment_picker.item_count:
		weights.resize(segment_picker.item_count)
		weights.fill(1)
	weights[segment_picker.selected] = value
	draft.set(_prefix() + "weights", weights)
	_record_edit(before)

func _texture_image(texture: Texture2D) -> Image:
	if texture is AtlasTexture:
		if texture.atlas == null:
			return null
		var page := _texture_image(texture.atlas)
		if page == null:
			return null
		var area: Rect2i = Rect2i(texture.region)
		if area.size == Vector2i.ZERO:
			area.size = page.get_size()
		var result := Image.create(texture.get_width(), texture.get_height(), false, Image.FORMAT_RGBA8)
		page.convert(Image.FORMAT_RGBA8)
		result.blit_rect(page, area, Vector2i(texture.margin.position))
		return result
	var image := texture.get_image()
	if image != null and image.is_compressed() and image.decompress() != OK:
		return null
	return image

func _trim() -> void:
	if draft == null:
		return
	var image := Image.new()
	if manager.source_path.get_extension().to_lower() not in ["png", "jpg", "jpeg", "webp", "bmp", "tga", "svg", "exr", "hdr"] or image.load(ProjectSettings.globalize_path(manager.source_path)) != OK:
		image = _texture_image(draft.texture)
	if image == null:
		status.text = "This texture cannot provide pixels for trimming."
		return
	if image.is_compressed() and image.decompress() != OK:
		status.text = "Cannot decompress this texture for trimming."
		return
	var region: Rect2i = draft.get_effective_region()
	if not region.has_area() or not Rect2i(Vector2i.ZERO, image.get_size()).encloses(region):
		status.text = "Set a region inside the image before trimming."
		return
	var used := image.get_region(region).get_used_rect()
	if used.has_area():
		_change_region(Rect2i(region.position + used.position, used.size))

func apply() -> void:
	if draft == null:
		return
	_change_name()
	var error: String = draft.get_validation_error()
	if not error.is_empty():
		status.text = error
		return
	if saved != null and not saved.resource_path.is_empty():
		_save_to(saved.resource_path)
		return
	var source: String = manager.source_path
	if not source.begins_with("res://") or source.contains("::"):
		save_dialog.current_file = "sprite.mpsprite.tres"
		save_dialog.popup_centered_ratio(0.6)
		return
	var path := source + ".mpsprite.tres"
	if FileAccess.file_exists(path):
		var base := source + "." + draft.resource_name.validate_filename()
		path = base + ".mpsprite.tres"
		var suffix := 2
		while FileAccess.file_exists(path):
			path = base + "_%d.mpsprite.tres" % suffix
			suffix += 1
	_save_to(path)

func _save_to(path: String) -> void:
	var target: Resource = saved if saved != null else ClassDB.instantiate("MultiPatchSprite")
	var old := _resource_state(target)
	var old_texture: Texture2D = target.texture
	for field in FIELDS:
		target.set(field, draft.get(field))
	target.texture = draft.texture
	var error := ResourceSaver.save(target, path)
	if error != OK:
		for field in FIELDS:
			target.set(field, old[field])
		target.texture = old_texture
		status.text = "Save failed: " + error_string(error)
		return
	target.take_over_path(path)
	saved = target
	baseline = _state()
	dirty = false
	EditorInterface.get_resource_filesystem().update_file(path)
	status.text = "Saved " + path
	if not target in siblings:
		siblings.append(target)
		sprite_picker.add_item(target.resource_name)
	sprite_picker.disabled = false
	sprite_picker.set_item_text(siblings.find(target), target.resource_name)
	sprite_picker.select(siblings.find(target))

func _unhandled_key_input(event: InputEvent) -> void:
	if not visible or draft == null or not event is InputEventKey or not event.pressed or event.echo:
		return
	var focus := get_viewport().gui_get_focus_owner()
	if focus == null or not is_ancestor_of(focus):
		return
	if event.is_command_or_control_pressed():
		if event.keycode == KEY_S:
			apply()
		elif event.keycode == KEY_Z:
			if event.shift_pressed: undo.redo()
			else: undo.undo()
		elif event.keycode == KEY_Y:
			undo.redo()
		else:
			return
	elif focus == canvas:
		match event.keycode:
			KEY_DELETE: _remove_cut()
			KEY_F: _set_mode(0)
			KEY_S: _set_mode(1)
			KEY_T: _set_mode(3 if event.shift_pressed else 2)
			_: return
	else:
		return
	get_viewport().set_input_as_handled()
