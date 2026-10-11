@tool
extends EditorPlugin

signal operation_finished(success: bool, message: String)
signal scene_path_chosen(path: String)

const EXTENSION := "res://bin/meowody.gdextension"
const LOG_PATH := "res://.godot/binding-build.log"
const CLASS_META := &"meowody_bind_class"
const MARK_META := &"meowody_bind"
enum Stage { IDLE, PREPARING, BUILDING, RELOADING, BINDING }

var stage := Stage.IDLE
var dock: EditorDock
var class_edit: LineEdit
var output_edit: LineEdit
var lifecycle: CheckBox
var nodes: Tree
var animation: OptionButton
var enter_edit: LineEdit
var exit_edit: LineEdit
var generate_button: Button
var feedback: Label
var directory: EditorFileDialog
var save_dialog: EditorFileDialog
var task_window: Window
var task_shade: ColorRect
var task_shade_layer: CanvasLayer
var task_title: Label
var task_hint: Label
var task_badge: Label
var task_steps: Array[Label] = []
var task_accent: Color
var task_error: Color
var task_muted: Color
var task_status: Label
var task_log: RichTextLabel
var close_button: Button
var progress: ProgressBar
var _build_pid := 0
var _root_id := 0
var _fingerprint := ""
var _refresh_elapsed := 0.0


func _enter_tree() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_create_dock()
	_create_task_window()
	scene_changed.connect(_scene_changed)
	set_process(true)
	_refresh_scene()
	if OS.get_cmdline_user_args().has("--test-binding-editor"):
		call_deferred("_run_tests")


func _exit_tree() -> void:
	set_process(false)
	if _build_pid > 0 and OS.is_process_running(_build_pid):
		OS.kill(_build_pid)
	task_window.free()
	task_shade_layer.free()
	directory.free()
	save_dialog.free()
	remove_dock(dock)
	dock.free()


# Never retain a native backend or native Callable across a DLL reload.
func _native(method: StringName, arguments: Array) -> Variant:
	if not ClassDB.class_exists("BindingEditorBackend"):
		return {"ok": false, "error": "Build the framework once to install BindingEditorBackend."}
	var backend = ClassDB.instantiate("BindingEditorBackend")
	return backend.callv(method, arguments)


func _field(box: VBoxContainer, title: String) -> LineEdit:
	var label := Label.new()
	label.text = title
	box.add_child(label)
	var edit := LineEdit.new()
	box.add_child(edit)
	return edit


func _create_dock() -> void:
	dock = EditorDock.new()
	dock.title = "C++ Scene Binding"
	dock.default_slot = EditorDock.DOCK_SLOT_RIGHT_BL
	var box := VBoxContainer.new()
	box.custom_minimum_size.x = 260
	dock.add_child(box)
	class_edit = _field(box, "Native class")
	output_edit = _field(box, "C++ output directory")
	output_edit.editable = false
	var browse := Button.new()
	browse.text = "Choose directory..."
	browse.pressed.connect(func(): directory.popup_centered_ratio(0.6))
	box.add_child(browse)
	lifecycle = CheckBox.new()
	lifecycle.text = "UI lifecycle (new classes)"
	box.add_child(lifecycle)
	nodes = Tree.new()
	nodes.columns = 2
	nodes.set_column_title(0, "Bind node")
	nodes.set_column_title(1, "C++ member")
	nodes.column_titles_visible = true
	nodes.custom_minimum_size.y = 240
	nodes.size_flags_vertical = Control.SIZE_EXPAND_FILL
	nodes.item_edited.connect(_node_toggled)
	box.add_child(nodes)
	var animation_label := Label.new()
	animation_label.text = "Transition AnimationPlayer (optional)"
	box.add_child(animation_label)
	animation = OptionButton.new()
	box.add_child(animation)
	enter_edit = _field(box, "Enter animation")
	exit_edit = _field(box, "Exit animation")
	generate_button = Button.new()
	generate_button.text = "Generate, Build and Bind"
	generate_button.pressed.connect(_generate)
	box.add_child(generate_button)
	feedback = Label.new()
	feedback.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(feedback)
	directory = EditorFileDialog.new()
	directory.access = EditorFileDialog.ACCESS_FILESYSTEM
	directory.file_mode = EditorFileDialog.FILE_MODE_OPEN_DIR
	directory.current_dir = ProjectSettings.globalize_path("res://../src")
	directory.dir_selected.connect(func(path: String): output_edit.text = path)
	add_child(directory)
	save_dialog = EditorFileDialog.new()
	save_dialog.file_mode = EditorFileDialog.FILE_MODE_SAVE_FILE
	save_dialog.add_filter("*.tscn", "Godot scene")
	save_dialog.file_selected.connect(func(path: String): scene_path_chosen.emit(path))
	save_dialog.canceled.connect(func(): scene_path_chosen.emit(""))
	add_child(save_dialog)
	add_dock(dock)


func _task_style(color: Color, radius: int = 8) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.set_corner_radius_all(radius)
	return style


func _create_task_window() -> void:
	var theme := EditorInterface.get_editor_theme()
	var scale := EditorInterface.get_editor_scale()
	task_accent = theme.get_color("accent_color", "Editor")
	task_error = theme.get_color("error_color", "Editor")
	task_muted = theme.get_color("font_color", "Editor").lerp(theme.get_color("base_color", "Editor"), 0.4)
	# CanvasLayer covers the entire main editor viewport, beneath the modal Window.
	task_shade_layer = CanvasLayer.new()
	task_shade_layer.layer = 100
	EditorInterface.get_base_control().add_child(task_shade_layer)
	task_shade = ColorRect.new()
	task_shade.hide()
	task_shade.color = Color(0.015, 0.02, 0.035, 0.65)
	task_shade.mouse_filter = Control.MOUSE_FILTER_STOP
	task_shade_layer.add_child(task_shade)
	task_shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	task_window = Window.new()
	task_window.hide()
	task_window.theme = theme
	task_window.title = "C++ Scene Binding"
	task_window.transient = true
	task_window.exclusive = true
	task_window.borderless = true
	task_window.unresizable = true
	task_window.size = Vector2i(Vector2(760, 480) * scale)
	task_window.close_requested.connect(_close_task_window)
	task_window.visibility_changed.connect(func(): task_shade.visible = task_window.visible)
	add_child(task_window)
	var panel := PanelContainer.new()
	var surface := _task_style(theme.get_color("base_color", "Editor"), 12)
	surface.set_border_width_all(1)
	surface.border_color = task_accent.darkened(0.55)
	panel.add_theme_stylebox_override("panel", surface)
	task_window.add_child(panel)
	panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var margin := MarginContainer.new()
	for side in ["left", "top", "right", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, roundi(24 * scale))
	panel.add_child(margin)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", roundi(16 * scale))
	margin.add_child(box)
	var heading := HBoxContainer.new()
	heading.add_theme_constant_override("separation", roundi(12 * scale))
	box.add_child(heading)
	var icon := TextureRect.new()
	icon.texture = theme.get_icon("Script", "EditorIcons")
	icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon.custom_minimum_size = Vector2(28, 28) * scale
	heading.add_child(icon)
	task_title = Label.new()
	task_title.text = "C++ Scene Binding"
	task_title.add_theme_font_size_override("font_size", roundi(22 * scale))
	task_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	heading.add_child(task_title)
	task_badge = Label.new()
	task_badge.add_theme_font_size_override("font_size", roundi(11 * scale))
	heading.add_child(task_badge)
	var steps := HBoxContainer.new()
	steps.add_theme_constant_override("separation", roundi(8 * scale))
	box.add_child(steps)
	for title in ["1  Generate", "2  Build", "3  Reload", "4  Bind"]:
		var step := Label.new()
		step.text = title
		step.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		step.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		step.custom_minimum_size.y = 36 * scale
		step.add_theme_font_size_override("font_size", roundi(13 * scale))
		steps.add_child(step)
		task_steps.append(step)
	task_status = Label.new()
	task_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(task_status)
	progress = ProgressBar.new()
	progress.show_percentage = false
	progress.custom_minimum_size.y = 5 * scale
	progress.add_theme_stylebox_override("background", _task_style(theme.get_color("dark_color_2", "Editor"), 3))
	progress.add_theme_stylebox_override("fill", _task_style(task_accent, 3))
	box.add_child(progress)
	var log_panel := PanelContainer.new()
	var log_style := _task_style(theme.get_color("dark_color_2", "Editor"))
	log_style.content_margin_left = 14 * scale
	log_style.content_margin_right = 14 * scale
	log_style.content_margin_top = 12 * scale
	log_style.content_margin_bottom = 12 * scale
	log_panel.add_theme_stylebox_override("panel", log_style)
	log_panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	box.add_child(log_panel)
	task_log = RichTextLabel.new()
	task_log.add_theme_font_override("normal_font", theme.get_font("source", "EditorFonts"))
	task_log.add_theme_font_size_override("normal_font_size", roundi(12 * scale))
	task_log.size_flags_vertical = Control.SIZE_EXPAND_FILL
	task_log.selection_enabled = true
	task_log.scroll_following = true
	log_panel.add_child(task_log)
	var footer := HBoxContainer.new()
	footer.add_theme_constant_override("separation", roundi(12 * scale))
	box.add_child(footer)
	task_hint = Label.new()
	task_hint.add_theme_color_override("font_color", task_muted)
	task_hint.add_theme_font_size_override("font_size", roundi(12 * scale))
	task_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	task_hint.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	footer.add_child(task_hint)
	var copy := Button.new()
	copy.text = "Copy output"
	copy.flat = true
	copy.pressed.connect(func(): DisplayServer.clipboard_set(task_log.get_parsed_text()))
	footer.add_child(copy)
	close_button = Button.new()
	close_button.text = "Close"
	close_button.custom_minimum_size = Vector2(88, 34) * scale
	close_button.pressed.connect(_close_task_window)
	footer.add_child(close_button)


func _paint_steps(current: int, failed: bool = false) -> void:
	for i in range(task_steps.size()):
		var active := i + 1 == current
		var done := i + 1 < current
		var color := task_error if active and failed else task_accent
		task_steps[i].add_theme_color_override("font_color", color if active or done else task_muted)
		task_steps[i].add_theme_stylebox_override("normal", _task_style(Color(color, 0.15 if active else 0.05)))


func _close_task_window() -> void:
	if stage == Stage.IDLE:
		task_window.hide()


func _scene_changed(_root: Node) -> void:
	if stage == Stage.IDLE:
		call_deferred("_refresh_scene")


func _scene_nodes(root: Node, node: Node, result: Array[Node]) -> void:
	result.append(node)
	if node != root and node.has_meta(CLASS_META):
		return
	for child in node.get_children():
		_scene_nodes(root, child, result)


func _refresh_scene() -> void:
	var root := EditorInterface.get_edited_scene_root()
	generate_button.disabled = root == null or stage != Stage.IDLE
	if stage != Stage.IDLE:
		return
	if root == null:
		nodes.clear()
		_root_id = 0
		return
	if not ClassDB.class_exists("BindingEditorBackend"):
		feedback.text = "Build the framework once to install the native binding backend."
		generate_button.disabled = true
		return
	var scene_nodes: Array[Node] = []
	_scene_nodes(root, root, scene_nodes)
	var fingerprint := ""
	for node in scene_nodes:
		fingerprint += "%s:%s:%s:%s;" % [node.get_instance_id(), node.name, root.get_path_to(node), node.get_meta(MARK_META, false)]
	var root_changed := _root_id != root.get_instance_id()
	var selected_player: NodePath = root.get_meta("meowody_ui_animation_player", NodePath())
	if not root_changed and animation.selected >= 0:
		selected_player = animation.get_selected_metadata()
	if root_changed:
		_root_id = root.get_instance_id()
		class_edit.text = root.get_meta(CLASS_META, _native("default_class_name", [root]))
		output_edit.text = root.get_meta("meowody_bind_output", "../src/ui/generated")
		lifecycle.button_pressed = root.get_meta("meowody_bind_ui", false)
		enter_edit.text = root.get_meta("meowody_ui_enter", "")
		exit_edit.text = root.get_meta("meowody_ui_exit", "")
		_fingerprint = ""
	if fingerprint == _fingerprint:
		return
	_fingerprint = fingerprint
	var description: Dictionary = _native("describe_scene", [root])
	var members := {}
	for binding in description.get("bindings", []):
		members[binding.path] = binding.member
	nodes.clear()
	animation.clear()
	animation.add_item("None")
	animation.set_item_metadata(0, NodePath())
	var rows := {}
	for node in scene_nodes:
		var item := nodes.create_item(rows.get(node.get_parent().get_instance_id()) if node != root else null)
		rows[node.get_instance_id()] = item
		var path := root.get_path_to(node)
		item.set_metadata(0, path)
		if node != root:
			item.set_cell_mode(0, TreeItem.CELL_MODE_CHECK)
			item.set_checked(0, node.get_meta(MARK_META, false))
			item.set_editable(0, true)
		# Changing cell mode resets its text; assign the name afterwards.
		item.set_text(0, node.name)
		item.set_text(1, members.get(path, ""))
		if node is AnimationPlayer and node != root:
			animation.add_item(str(path))
			var index := animation.item_count - 1
			animation.set_item_metadata(index, path)
			if path == selected_player:
				animation.select(index)


func _meta_action(node: Node, key: StringName, value: Variant) -> void:
	var undo := get_undo_redo()
	undo.add_do_method(node, "set_meta", key, value)
	if node.has_meta(key):
		undo.add_undo_method(node, "set_meta", key, node.get_meta(key))
	else:
		undo.add_undo_method(node, "remove_meta", key)


func _node_toggled() -> void:
	var root := EditorInterface.get_edited_scene_root()
	var item := nodes.get_edited()
	if root == null or item == null or stage != Stage.IDLE:
		return
	var node := root.get_node_or_null(item.get_metadata(0))
	if node == null or node == root:
		return
	get_undo_redo().create_action("C++ node binding", UndoRedo.MERGE_DISABLE, root)
	_meta_action(node, MARK_META, item.is_checked(0))
	get_undo_redo().commit_action()
	_fingerprint = ""
	# Tree is locked while item_edited is being emitted by a mouse event.
	call_deferred("_refresh_scene")


func _configure(root: Node) -> void:
	get_undo_redo().create_action("C++ scene binding settings", UndoRedo.MERGE_DISABLE, root)
	var name_changed: bool = root.get_meta(CLASS_META, "") != class_edit.text.strip_edges()
	if not root.has_meta("meowody_bind_base") or name_changed:
		_meta_action(root, &"meowody_bind_base", root.get_class())
	_meta_action(root, CLASS_META, class_edit.text.strip_edges())
	_meta_action(root, &"meowody_bind_output", output_edit.text)
	_meta_action(root, &"meowody_bind_ui", lifecycle.button_pressed)
	_meta_action(root, &"meowody_ui_enter", enter_edit.text)
	_meta_action(root, &"meowody_ui_exit", exit_edit.text)
	var player_path: NodePath = animation.get_selected_metadata() if animation.selected >= 0 else NodePath()
	_meta_action(root, &"meowody_ui_animation_player", player_path)
	var scene_nodes: Array[Node] = []
	_scene_nodes(root, root, scene_nodes)
	for node in scene_nodes:
		var selected := not player_path.is_empty() and root.get_path_to(node) == player_path
		if selected or node.has_meta("meowody_ui_animation"):
			_meta_action(node, &"meowody_ui_animation", selected)
		if selected:
			_meta_action(node, MARK_META, true)
	get_undo_redo().commit_action()
	# Old versions persisted build state in scenes. It is no longer consulted.
	for key in ["meowody_bind_auto_apply", "meowody_bind_build_hash", "meowody_bind_editor_pid"]:
		if root.has_meta(key):
			root.remove_meta(key)


func _set_stage(value: Stage, message: String) -> void:
	stage = value
	if value != Stage.IDLE:
		task_title.text = "C++ Scene Binding"
		task_badge.text = "IN PROGRESS"
		task_badge.add_theme_color_override("font_color", task_accent)
		task_status.remove_theme_color_override("font_color")
		task_hint.text = "Closes automatically when all four steps succeed."
		_paint_steps(value)
	task_status.text = message
	feedback.text = message
	_append_log(message + "\n")


func _append_log(text: String) -> void:
	task_log.add_text(text)


func _finish(success: bool, message: String) -> void:
	var finished_stage := stage
	_set_stage(Stage.IDLE, message)
	progress.indeterminate = false
	progress.value = 100 if success else 0
	close_button.visible = not success
	if success:
		task_window.hide()
	else:
		task_title.text = "Build failed" if finished_stage == Stage.BUILDING else "Scene binding failed"
		task_badge.text = "FAILED"
		task_badge.add_theme_color_override("font_color", task_error)
		task_status.add_theme_color_override("font_color", task_error)
		task_hint.text = "Review or copy the output, then close this window."
		_paint_steps(finished_stage, true)
		# A failure is terminal, but the modal and shade stay until explicit dismissal.
		close_button.grab_focus()
	_fingerprint = ""
	_refresh_scene()
	operation_finished.emit(success, message)


func _generate() -> void:
	if stage != Stage.IDLE or task_window.visible:
		return
	var root := EditorInterface.get_edited_scene_root()
	if root == null:
		return
	if EditorInterface.is_playing_scene():
		feedback.text = "Stop the running game before building native bindings."
		return
	if not ClassDB.class_exists("BindingEditorBackend"):
		feedback.text = "Build the framework once before using this plugin."
		return
	if not _native("valid_class_name", [class_edit.text.strip_edges()]):
		feedback.text = "Enter a valid C++ class name."
		return
	if root.get_script() != null:
		feedback.text = "Remove the root script before converting it to a native C++ class."
		return
	stage = Stage.PREPARING
	generate_button.disabled = true
	if root.scene_file_path.is_empty():
		save_dialog.current_file = str(root.name).to_snake_case() + ".tscn"
		save_dialog.popup_centered_ratio(0.6)
		var chosen: String = await scene_path_chosen
		if chosen.is_empty():
			stage = Stage.IDLE
			_refresh_scene()
			return
		EditorInterface.save_scene_as(chosen)
	var path := root.scene_file_path
	if path.is_empty() or path.get_extension() != "tscn":
		stage = Stage.IDLE
		feedback.text = "Save this scene as a .tscn file."
		_refresh_scene()
		return
	task_log.clear()
	close_button.hide()
	progress.indeterminate = true
	task_window.popup_centered_clamped(Vector2i(Vector2(760, 480) * EditorInterface.get_editor_scale()), 0.85)
	_set_stage(Stage.PREPARING, "1/4  Saving scene and generating C++...")
	await get_tree().process_frame
	_configure(root)
	if EditorInterface.save_scene() != OK:
		_finish(false, "Could not save the scene.")
		return
	var generated: Dictionary = _native("generate_scene", [root])
	if not generated.ok:
		_finish(false, generated.error)
		return
	var native_class := class_edit.text.strip_edges()
	var description: Dictionary = _native("describe_scene", [root])
	var scene_hash := FileAccess.get_md5(path)
	root = null
	var platform: String = {"Windows": "windows", "Linux": "linux"}.get(OS.get_name(), "")
	if platform.is_empty():
		_finish(false, "Automatic SCons builds currently support Windows and Linux.")
		return
	var suffix := "dll" if platform == "windows" else "so"
	var library := "libmeowody.%s.template_debug.%s.%s" % [platform, Engine.get_architecture_name(), suffix]
	# Keep each PDB at its original link path while that DLL is loaded.
	var output_dir := "res://.godot/binding-build/%s" % Time.get_ticks_usec()
	var staged := output_dir + "/" + library
	var live := "res://bin/" + library
	_set_stage(Stage.BUILDING, "2/4  Building C++ (SCons)...")
	var python := "python" if platform == "windows" else "python3"
	_build_pid = OS.create_process(python, PackedStringArray([
		ProjectSettings.globalize_path("res://addons/meowody_binding/build.py"),
		ProjectSettings.globalize_path("res://.."), ProjectSettings.globalize_path(LOG_PATH),
		ProjectSettings.globalize_path(output_dir)]))
	if _build_pid <= 0:
		_finish(false, "Cannot start Python. Make sure Python and SCons are on the editor's PATH.")
		return
	while OS.is_process_running(_build_pid):
		await get_tree().create_timer(0.1).timeout
	var exit_code := OS.get_process_exit_code(_build_pid)
	_build_pid = 0
	_append_log(_build_tail() + "\n")
	if exit_code != 0:
		_finish(false, "SCons failed (exit %s). Full output: %s" % [exit_code, LOG_PATH])
		return
	if not FileAccess.file_exists(staged):
		_finish(false, "SCons did not produce the expected library: " + staged)
		return
	if FileAccess.get_md5(path) != scene_hash:
		_finish(false, "The scene changed during the build. Generate again to use the latest scene.")
		return
	_set_stage(Stage.RELOADING, "3/4  Loading the compiled GDExtension...")
	await get_tree().process_frame
	var reload_error := _publish_and_reload(staged, live)
	if not reload_error.is_empty():
		_finish(false, reload_error)
		return
	if not ClassDB.class_exists(native_class):
		_finish(false, "The reloaded library did not register " + native_class + ". Check the build output.")
		return
	_set_stage(Stage.BINDING, "4/4  Applying native root and node references...")
	var backup := path.get_basename() + ".before-native-%s.tscn.bak" % Time.get_ticks_usec()
	var binding_error: String = preload("scene_binding.gd").apply(path, native_class, description.bindings, backup)
	if not binding_error.is_empty():
		_finish(false, binding_error)
		return
	EditorInterface.reload_scene_from_path(path)
	var deadline := Time.get_ticks_msec() + 10000
	while Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
		var edited := EditorInterface.get_edited_scene_root()
		if edited != null and edited.scene_file_path == path and edited.get_class() == native_class:
			var cleanup_error := DirAccess.remove_absolute(backup)
			if cleanup_error != OK:
				_finish(false, "Scene bound, but could not remove backup: %s (%s)" % [backup, error_string(cleanup_error)])
				return
			_finish(true, "Bound %s and saved %s." % [native_class, path.get_file()])
			return
	_finish(false, "Bindings were saved, but the editor could not reopen the scene. Reopen " + path)


func _build_tail() -> String:
	var file := FileAccess.open(LOG_PATH, FileAccess.READ)
	if file == null:
		return "No SCons log was written."
	file.seek(maxi(0, file.get_length() - 8192))
	if file.get_position() > 0:
		file.get_line() # Skip the partial first line.
	var lines := file.get_buffer(file.get_length() - file.get_position()).get_string_from_utf8().strip_edges().split("\n")
	return "\n".join(lines.slice(maxi(0, lines.size() - 30)))


func _publish_and_reload(staged: String, live: String) -> String:
	if not GDExtensionManager.is_extension_loaded(EXTENSION):
		return "The framework GDExtension is not loaded."
	var backup := "res://.godot/binding-build/previous-library"
	if DirAccess.copy_absolute(live, backup) != OK:
		return "Cannot back up the current library."
	var temporary := live + ".binding-tmp"
	var pdb := live.get_basename() + ".pdb"
	var staged_pdb := staged.get_basename() + ".pdb"
	if DirAccess.copy_absolute(staged, temporary) != OK:
		return "Cannot stage the compiled library."
	if DirAccess.rename_absolute(temporary, live) != OK:
		DirAccess.remove_absolute(temporary)
		return "Cannot replace the library. Check file permissions and other running builds."
	# This call runs in GDScript; no frame or Callable from our DLL is on the stack.
	var status := GDExtensionManager.reload_extension(EXTENSION)
	if status != GDExtensionManager.LOAD_STATUS_OK:
		var restored := DirAccess.copy_absolute(backup, temporary)
		if restored == OK:
			restored = DirAccess.rename_absolute(temporary, live)
		if restored == OK:
			GDExtensionManager.reload_extension(EXTENSION)
		return "GDExtension reload failed (status %s). Previous library restore: %s." % [status, error_string(restored)]
	GDExtensionManager.emit_signal("extensions_reloaded")
	if FileAccess.file_exists(staged_pdb) and DirAccess.copy_absolute(staged_pdb, pdb) != OK:
		_append_log("Debug symbols remain available at " + staged_pdb + "\n")
	return ""


func _process(delta: float) -> void:
	if stage != Stage.IDLE:
		return
	_refresh_elapsed += delta
	if _refresh_elapsed >= 0.5:
		_refresh_elapsed = 0.0
		_refresh_scene()


func _run_tests() -> void:
	var test = load("res://tests/meowody_binding/editor_test.gd").new()
	await test.run(self)
