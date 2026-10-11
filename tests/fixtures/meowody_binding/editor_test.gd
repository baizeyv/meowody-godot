@tool
extends RefCounted

const SCENE := "res://binding_editor_test.tscn"
const OUTPUT := "res://../src/ui/generated/binding_editor_test"
var failures: Array[String] = []
var result := {}
var modal_checked := false


func check(condition: bool, message: String) -> void:
	print("[BindingTest] ", "PASS " if condition else "FAIL ", message)
	if not condition:
		failures.append(message)


func _finished(success: bool, message: String) -> void:
	result = {"ok": success, "message": message}
	print("[BindingTest] operation: ", message)


func _check_running_window(plugin: EditorPlugin) -> void:
	check(plugin.stage != plugin.Stage.IDLE, "build runs asynchronously")
	plugin.task_window.close_requested.emit()
	check(plugin.task_window.visible and not plugin.close_button.visible, "running modal cannot be closed")
	check(plugin.task_shade.visible and plugin.task_shade.mouse_filter == Control.MOUSE_FILTER_STOP, "running modal dims and blocks the editor")
	check(plugin.task_shade.size.is_equal_approx(plugin.task_shade.get_viewport_rect().size), "shade covers the entire editor viewport")
	modal_checked = true


func _click_checkbox(plugin: EditorPlugin) -> void:
	var row: TreeItem = plugin.nodes.get_root().get_first_child()
	var rect: Rect2 = plugin.nodes.get_item_area_rect(row, 0)
	var position: Vector2 = plugin.nodes.get_global_transform_with_canvas() * (rect.position + Vector2(8, rect.size.y / 2))
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		event.position = position
		plugin.nodes.get_viewport().push_input(event, true)
	await plugin.get_tree().process_frame


func _check_native_connections(controls: Array[Node]) -> void:
	for control in controls:
		var signals: Array = ["button_down", "button_up", "mouse_exited", "focus_exited"] if control is Button else ["scroll_started", "scroll_ended"]
		var valid := true
		for signal_name in signals:
			valid = valid and control.get_signal_connection_list(signal_name).size() == 1
		check(valid, "%s keeps one native connection per signal across DLL reload" % control.get_class())


func run(plugin: EditorPlugin) -> void:
	await plugin.get_tree().create_timer(1.0).timeout
	check(plugin.stage == plugin.Stage.IDLE and not plugin.task_window.visible, "startup is idle")
	check(not plugin.task_shade.visible, "startup does not shade the editor")
	if FileAccess.file_exists(SCENE) or DirAccess.dir_exists_absolute(OUTPUT):
		push_error("Binding test fixture already exists. Run the test in a fresh scratch project.")
		plugin.get_tree().quit(1)
		return
	check(not ClassDB.class_exists("BindingEditorTestView"), "test begins with an unregistered native class")
	var reload_controls: Array[Node] = []
	for native_type in ["LongPressButton", "RepeatButton", "SnapScrollContainer", "LoopScrollContainer"]:
		reload_controls.append(ClassDB.instantiate(native_type))
	_check_native_connections(reload_controls)
	var root := Control.new()
	root.name = "BindingEditorTest"
	root.set_meta("meowody_bind_class", "BindingEditorTestView")
	root.set_meta("meowody_bind_output", "res://addons/meowody_binding/src/ui/generated/binding_editor_test")
	root.set_meta("meowody_bind_ui", true)
	root.set_meta("meowody_bind_auto_apply", true)
	root.set_meta("meowody_bind_editor_pid", str(OS.get_process_id()))
	var button := Button.new()
	button.name = "Action"
	root.add_child(button)
	button.owner = root
	button.set_meta("meowody_bind", true)
	var packed := PackedScene.new()
	packed.pack(root)
	ResourceSaver.save(packed, SCENE)
	root.free()
	EditorInterface.open_scene_from_path(SCENE)
	await plugin.get_tree().create_timer(0.5).timeout
	plugin._refresh_scene()
	check(not plugin.task_window.visible, "legacy scene markers never open a task window")
	check(plugin.class_edit.text == "BindingEditorTestView", "scene settings loaded")
	plugin.dock.make_visible()
	await plugin.get_tree().process_frame
	check(plugin.nodes.get_root().get_first_child().get_text(0) == "Action", "checkbox row shows the node name")
	var action := EditorInterface.get_edited_scene_root().get_node("Action")
	action.name = "RenamedAction"
	plugin._refresh_scene()
	var renamed: TreeItem = plugin.nodes.get_root().get_first_child()
	check(renamed.get_text(0) == "RenamedAction" and renamed.get_text(1) == "m_renamed_action", "rename updates node label and C++ member together")
	action.name = "Action"
	plugin._refresh_scene()
	await _click_checkbox(plugin)
	check(not EditorInterface.get_edited_scene_root().get_node("Action").get_meta("meowody_bind"), "mouse click unchecks a binding without rebuilding a locked Tree")
	await _click_checkbox(plugin)
	check(EditorInterface.get_edited_scene_root().get_node("Action").get_meta("meowody_bind"), "second mouse click restores binding")
	plugin.operation_finished.connect(_finished)
	var plugin_id := plugin.get_instance_id()
	var window_id: int = plugin.task_window.get_instance_id()
	plugin.get_tree().create_timer(0.25).timeout.connect(_check_running_window.bind(plugin))
	await plugin._generate()
	_check_native_connections(reload_controls)
	check(result.get("ok", false), "first generation builds, hot reloads and binds")
	check(modal_checked, "modal was checked during a real build")
	check(plugin.get_instance_id() == plugin_id and plugin.task_window.get_instance_id() == window_id,
		"editor and modal survive DLL reload")
	check(not plugin.task_window.visible, "success closes the modal")
	check(not plugin.task_shade.visible, "success removes the editor shade")
	var edited := EditorInterface.get_edited_scene_root()
	check(edited.get_class() == "BindingEditorTestView", "scene root converted to new native class")
	if result.get("ok", false):
		check(edited.get("m_action") == edited.get_node("Action"), "node reference is bound")
		var saved: PackedScene = ResourceLoader.load(SCENE, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
		var instance := saved.instantiate()
		check(instance.get_class() == "BindingEditorTestView", "saved scene has native root type")
		check(instance.get("m_action") == instance.get_node("Action"), "saved node reference survives fresh instantiation")
		instance.free()
		var members := FileAccess.get_file_as_string(OUTPUT + "/BindingEditorTestView.bindings.gen.inc")
		check(not members.contains("NativeScene") and not members.contains("validate_bindings"), "generated members need no runtime registry")
		check(not edited.has_meta("meowody_bind_auto_apply"), "transient state is absent from saved scenes")
		var added := Label.new()
		added.name = "Status"
		edited.add_child(added)
		added.owner = edited
		added.set_meta("meowody_bind", true)
		# Exercise the real C++ UI center with ordinary generated fields and hooks.
		var header_path := OUTPUT + "/BindingEditorTestView.h"
		var header := FileAccess.get_file_as_string(header_path)
		var writer := FileAccess.open(header_path, FileAccess.WRITE)
		writer.store_string(header.replace("};", "    GDMETHOD bool test_ui_center();\n};"))
		writer = null
		var cpp_path := OUTPUT + "/BindingEditorTestView.cpp"
		var cpp := FileAccess.get_file_as_string(cpp_path).replace("// Apply this opening's data here.", "if (m_action) m_action->set_text(data);")
		cpp = cpp.replace("void BindingEditorTestView::on_close() {", "void BindingEditorTestView::on_close() {\n    set_meta(\"closed\", true);")
		cpp += """
#include "runtime/ui/ui_center.h"
bool BindingEditorTestView::test_ui_center() {
    meowody::ResourceCenter resources;
    meowody::UICenter center(resources);
    auto handle = center.open(SCENE_PATH, "bound in C++");
    auto* view = godot::Object::cast_to<BindingEditorTestView>(handle.root());
    bool ok = handle.is_open() && view && view->m_action && view->m_action->get_text() == "bound in C++";
    center.close(handle.id());
    ok = ok && bool(view->get_meta("closed", false));
    center.shutdown();
    return ok;
}
"""
		writer = FileAccess.open(cpp_path, FileAccess.WRITE)
		writer.store_string(cpp)
		writer = null
		await plugin._generate()
		_check_native_connections(reload_controls)
		check(result.get("ok", false), "second generation adds a member without restarting")
		edited = EditorInterface.get_edited_scene_root()
		if result.get("ok", false):
			check(edited.get("m_status") == edited.get_node("Status"), "new member bound after reload")
			check(edited.call("test_ui_center"), "UICenter calls hooks and C++ uses serialized node properties without a registry")
		# A genuine compiler failure must not replace the last working DLL.
		var source := OUTPUT + "/BindingEditorTestView.cpp"
		var original := FileAccess.get_file_as_string(source)
		var file := FileAccess.open(source, FileAccess.WRITE)
		file.store_string(original + "\n#error Intentional binding editor regression test\n")
		file = null
		var live := "res://bin/libmeowody.windows.template_debug.x86_64.dll"
		var before := FileAccess.get_md5(live)
		await plugin._generate()
		check(not result.get("ok", true), "compiler failure terminates the operation")
		check(FileAccess.get_md5(live) == before, "compiler failure preserves the working DLL")
		check(plugin.stage == plugin.Stage.IDLE and plugin.close_button.visible, "failure unlocks the editor")
		await plugin.get_tree().create_timer(0.5).timeout
		check(plugin.task_window.visible and plugin.task_shade.visible, "compiler failure keeps the modal and shade until dismissed")
		check(plugin.task_badge.text == "FAILED" and not plugin.task_log.get_parsed_text().is_empty(), "failure retains readable output and an explicit error state")
		plugin._close_task_window()
		check(not plugin.task_window.visible, "failure window can be dismissed")
		check(not plugin.task_shade.visible, "manual dismissal removes the shade")
		file = FileAccess.open(source, FileAccess.WRITE)
		file.store_string(original)
		file = null
		await plugin._generate()
		check(result.get("ok", false), "retry succeeds after fixing compiler error")
		_check_native_connections(reload_controls)
	for control in reload_controls:
		control.free()
	reload_controls.clear()
	check(true, "native signal callables free safely after repeated DLL replacements")
	print("[BindingTest] failures=", failures.size())
	# Let the editor finish deferred scene/inspector cleanup before shutting it down.
	await plugin.get_tree().create_timer(1.0).timeout
	plugin.get_tree().quit(0 if failures.is_empty() else 1)
