@tool
extends VBoxContainer
const Settings = preload("settings.gd")
var manager: EditorPlugin
var data := {}
var group_name := ""
var groups: Tree
var entries: Tree
var search: LineEdit
var profile: OptionButton
var platform: OptionButton
var mode: OptionButton
var remote: CheckBox
var startup: CheckBox
var destination: OptionButton
var fields := {}
var output: TextEdit
var refreshing := false

func _ready() -> void:
	data = Settings.load_settings()
	var toolbar := HBoxContainer.new()
	add_child(toolbar)
	_button(toolbar, "Save", save)
	_button(toolbar, "Add group", _new_group)
	_button(toolbar, "Remove group", _remove_group)
	_button(toolbar, "Remove resource", _remove_entry)
	_button(toolbar, "Locate resource", _locate)
	profile = OptionButton.new()
	toolbar.add_child(profile)
	profile.item_selected.connect(_select_profile)
	platform = OptionButton.new()
	for name in ["windows", "android", "ios"]: platform.add_item(name)
	toolbar.add_child(platform)
	_button(toolbar, "Analyze", func(): _build(true))
	_button(toolbar, "Build content", func(): _build(false))
	_button(toolbar, "Cancel build", func(): manager.cancel_build())
	mode = OptionButton.new()
	mode.add_item("Play: Source resources")
	mode.add_item("Play: Real packages")
	toolbar.add_child(mode)
	mode.item_selected.connect(func(index: int):
		var next := data.duplicate(true)
		next.play_mode = "source" if index == 0 else "packed"
		change(next, "Change resource play mode"))
	search = LineEdit.new()
	search.placeholder_text = "Search addresses, labels and paths; drag resources from FileSystem into the list"
	add_child(search)
	search.text_changed.connect(func(_text: String): refresh())
	var split := HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_child(split)
	groups = Tree.new()
	groups.custom_minimum_size.x = 180
	groups.hide_root = true
	split.add_child(groups)
	groups.item_selected.connect(_select_group)
	entries = Tree.new()
	entries.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	entries.hide_root = true
	entries.columns = 3
	entries.column_titles_visible = true
	for index in range(3): entries.set_column_title(index, ["Address", "Source resource", "Labels (comma separated)"][index])
	split.add_child(entries)
	entries.item_edited.connect(_edit_entry)
	entries.set_drag_forwarding(Callable(), _can_drop, _drop)
	var properties := VBoxContainer.new()
	properties.custom_minimum_size.x = 260
	split.add_child(properties)
	remote = CheckBox.new()
	remote.text = "Remote group"
	properties.add_child(remote)
	startup = CheckBox.new()
	startup.text = "Download at startup"
	properties.add_child(startup)
	remote.toggled.connect(func(value: bool): _group_property("remote", value))
	startup.toggled.connect(func(value: bool): _group_property("startup", value))
	properties.add_child(_label("Move selected resource to group"))
	destination = OptionButton.new()
	properties.add_child(destination)
	destination.item_selected.connect(_move_entry)
	for key in ["remote_root", "output", "windows", "android", "ios", "compatibility"]:
		var labels := {"remote_root": "Remote server URL", "output": "Build output directory", "windows": "Windows export preset", "android": "Android export preset", "ios": "iOS export preset", "compatibility": "Content compatibility version"}
		properties.add_child(_label(labels[key]))
		var field := LineEdit.new()
		properties.add_child(field)
		fields[key] = field
		field.text_submitted.connect(func(_text: String): _profile_fields())
		field.focus_exited.connect(_profile_fields)
	output = TextEdit.new()
	output.editable = false
	output.custom_minimum_size.y = 150
	add_child(output)
	refresh()

func _label(text: String) -> Label:
	var label := Label.new()
	label.text = text
	return label

func _button(parent: Control, text: String, action: Callable) -> void:
	var button := Button.new()
	button.text = text
	parent.add_child(button)
	button.pressed.connect(action)

func change(next: Dictionary, action: String) -> void:
	if next == data: return
	var undo := manager.get_undo_redo()
	undo.create_action(action, UndoRedo.MERGE_DISABLE, self)
	undo.add_do_method(self, "set_data", next.duplicate(true))
	undo.add_undo_method(self, "set_data", data.duplicate(true))
	undo.commit_action()

func set_data(value: Dictionary) -> void:
	data = value.duplicate(true)
	refresh()
	output.text = "Unsaved resource configuration"

func refresh() -> void:
	if groups == null: return
	refreshing = true
	groups.clear()
	entries.clear()
	profile.clear()
	destination.clear()
	var root := groups.create_item()
	var index := 0
	for group in data.get("groups", []):
		var item := groups.create_item(root)
		item.set_text(0, group.name)
		item.set_metadata(0, group.name)
		destination.add_item(group.name)
		if group_name.is_empty(): group_name = group.name
		if group_name == group.name:
			item.select(0)
			remote.set_pressed_no_signal(group.get("remote", false))
			startup.set_pressed_no_signal(group.get("startup", false))
			destination.select(index)
		index += 1
	index = 0
	for name in data.get("profiles", {}):
		profile.add_item(name)
		if name == data.active_profile: profile.select(index)
		index += 1
	mode.select(0 if data.get("play_mode", "source") == "source" else 1)
	var selected: Dictionary = data.get("profiles", {}).get(data.get("active_profile", ""), {})
	for key in fields:
		fields[key].text = str(data.get(key, "")) if key == "compatibility" else str(selected.get("presets", {}).get(key, "")) if key in ["windows", "android", "ios"] else str(selected.get(key, ""))
	root = entries.create_item()
	for i in range(data.get("entries", []).size()):
		var entry: Dictionary = data.entries[i]
		if entry.get("group") != group_name: continue
		if not search.text.is_empty() and not JSON.stringify(entry).to_lower().contains(search.text.to_lower()): continue
		var item := entries.create_item(root)
		item.set_metadata(0, i)
		item.set_text(0, entry.get("address", ""))
		item.set_text(1, entry.get("path", ""))
		item.set_text(2, ", ".join(entry.get("labels", [])))
		item.set_editable(0, true)
		item.set_editable(2, true)
	refreshing = false

func _select_group() -> void:
	if refreshing or groups.get_selected() == null: return
	group_name = groups.get_selected().get_metadata(0)
	refresh()

func _select_profile(index: int) -> void:
	if refreshing: return
	var next := data.duplicate(true)
	next.active_profile = profile.get_item_text(index)
	change(next, "Select resource profile")

func _profile_fields() -> void:
	if refreshing: return
	var next := data.duplicate(true)
	for key in fields:
		if key == "compatibility": next[key] = fields[key].text
		elif key in ["windows", "android", "ios"]: next.profiles[next.active_profile].presets[key] = fields[key].text
		else: next.profiles[next.active_profile][key] = fields[key].text
	change(next, "Edit resource profile")

func _group_property(key: String, value: bool) -> void:
	if refreshing: return
	var next := data.duplicate(true)
	for group in next.groups:
		if group.name == group_name: group[key] = value
	change(next, "Edit resource group")

func _edit_entry() -> void:
	var item := entries.get_edited()
	if item == null: return
	var next := data.duplicate(true)
	var entry: Dictionary = next.entries[item.get_metadata(0)]
	entry.address = item.get_text(0)
	entry.labels = []
	for label in item.get_text(2).split(",", false):
		if not label.strip_edges().is_empty() and label.strip_edges() not in entry.labels: entry.labels.append(label.strip_edges())
	change(next, "Edit resource address")

func _can_drop(_position: Vector2, value: Variant) -> bool:
	return value is Dictionary and value.get("type") in ["files", "files_and_dirs"] and not group_name.is_empty()

func _drop(_position: Vector2, value: Variant) -> void:
	var next := data.duplicate(true)
	for path in value.get("files", []):
		if not ResourceLoader.exists(path): continue
		var exists := false
		for entry in next.entries:
			if entry.path == path: exists = true
		if exists: continue
		var uid := ResourceLoader.get_resource_uid(path)
		next.entries.append({"address": path.trim_prefix("res://").get_basename(), "path": path, "uid": ResourceUID.id_to_text(uid), "group": group_name, "labels": []})
	change(next, "Register addressable resources")

func _new_group() -> void:
	var dialog := ConfirmationDialog.new()
	dialog.title = "New resource group"
	var name_field := LineEdit.new()
	name_field.placeholder_text = "Unique group name"
	dialog.add_child(name_field)
	add_child(dialog)
	dialog.confirmed.connect(func():
		var name := name_field.text.strip_edges()
		if name.is_empty(): return
		for group in data.groups:
			if group.name == name: return
		var next := data.duplicate(true)
		next.groups.append({"name": name, "remote": false, "startup": false})
		group_name = name
		change(next, "Add resource group"))
	dialog.visibility_changed.connect(func():
		if not dialog.visible: dialog.queue_free())
	dialog.popup_centered(Vector2i(350, 100))

func _remove_group() -> void:
	for entry in data.entries:
		if entry.group == group_name:
			output.text = "Move or remove this group's resources first"
			return
	var next := data.duplicate(true)
	next.groups = next.groups.filter(func(group): return group.name != group_name)
	group_name = ""
	change(next, "Remove resource group")

func _remove_entry() -> void:
	var item := entries.get_selected()
	if item == null: return
	var next := data.duplicate(true)
	next.entries.remove_at(item.get_metadata(0))
	change(next, "Remove addressable resource")

func _move_entry(index: int) -> void:
	if refreshing or entries.get_selected() == null: return
	var next := data.duplicate(true)
	next.entries[entries.get_selected().get_metadata(0)].group = destination.get_item_text(index)
	change(next, "Move addressable resource")

func _locate() -> void:
	var item := entries.get_selected()
	if item != null: EditorInterface.get_file_system_dock().navigate_to_path(data.entries[item.get_metadata(0)].path)

func save() -> bool:
	var errors := Settings.resolve(data)
	if not errors.is_empty():
		output.text = "\n".join(errors)
		return false
	var error := Settings.save_json(Settings.PATH, data)
	output.text = "Saved resource configuration" if error == OK else "Save failed: " + error_string(error)
	return error == OK

func _build(analyze_only: bool) -> void:
	if save(): manager.start_build(data.active_profile, platform.get_item_text(platform.selected), analyze_only)
