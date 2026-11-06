@tool
extends RefCounted
const Settings = preload("res://addons/meowody_resources/settings.gd")
const Builder = preload("res://addons/meowody_resources/builder.gd")
var failures := 0
func check(value: bool, message: String) -> void:
	print("[ResourceEditorTest] ", "PASS " if value else "FAIL ", message)
	if not value: failures += 1

func run(plugin: EditorPlugin) -> void:
	var panel: Control = plugin.panel
	EditorInterface.set_main_screen_editor("Resources")
	await plugin.get_tree().process_frame
	var original: Dictionary = panel.data.duplicate(true)
	var settings := Settings.defaults()
	settings.groups.append({"name": "Remote", "remote": true, "startup": false})
	panel.set_data(settings)
	panel.group_name = "Local"
	panel.refresh()
	check(panel._can_drop(Vector2.ZERO, {"type": "files", "files": ["res://item.tres"]}), "filesystem resource drag accepted")
	panel._drop(Vector2.ZERO, {"files": ["res://item.tres"]})
	check(panel.data.entries.size() == 1 and panel.data.entries[0].group == "Local", "drop registers address and group")
	var undo := plugin.get_undo_redo()
	var history := undo.get_history_undo_redo(undo.get_object_history_id(panel))
	history.undo()
	check(panel.data.entries.is_empty(), "undo resource registration")
	history.redo()
	check(panel.data.entries.size() == 1, "redo resource registration")
	var next: Dictionary = panel.data.duplicate(true)
	next.entries[0].address = "test/item"
	next.entries[0].labels = ["sample", "startup"]
	panel.change(next, "Edit test address and labels")
	check(panel.save(), "save resource configuration")
	check(Settings.load_settings().entries[0].address == "test/item", "reopen saved address and labels")
	var duplicate: Dictionary = panel.data.duplicate(true)
	duplicate.entries.append(duplicate.entries[0].duplicate(true))
	check(not Settings.resolve(duplicate).is_empty(), "duplicate addresses rejected")
	var analysis := Builder.analyze(panel.data)
	check(analysis.errors.is_empty() and analysis.packs.size() == 1, "analyze configured resources")
	await plugin.get_tree().process_frame
	if not DisplayServer.get_name() == "headless":
		RenderingServer.force_draw(false)
		plugin.get_viewport().get_texture().get_image().save_png("res://resource_editor.png")
	Settings.save_json(Settings.PATH, original)
	panel.set_data(original)
	print("[ResourceEditorTest] failures=", failures)
	plugin.get_tree().quit(failures)
