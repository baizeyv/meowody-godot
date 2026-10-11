@tool
extends EditorPlugin
var inspector: EditorInspectorPlugin
var exporter: EditorExportPlugin
func _enter_tree() -> void:
	if not ClassDB.class_exists("VirtualList"):
		push_error("Common Nodes: run scons and reopen the editor to load the native classes.")
		return
	inspector = preload("inspector.gd").new()
	inspector.plugin = self
	add_inspector_plugin(inspector)
	exporter = preload("export_plugin.gd").new()
	add_export_plugin(exporter)
	add_tool_menu_item("Meowody Common Nodes Gallery", _open_gallery)
	if OS.get_cmdline_user_args().has("--test-common-nodes"):
		_run_tests.call_deferred()
func _exit_tree() -> void:
	if inspector != null:
		remove_inspector_plugin(inspector)
		remove_export_plugin(exporter)
		remove_tool_menu_item("Meowody Common Nodes Gallery")
func _open_gallery() -> void:
	EditorInterface.open_scene_from_path("res://addons/meowody_nodes/examples/gallery.tscn")
func _run_tests() -> void:
	await load("res://tests/meowody_nodes/editor_test.gd").new().run(self)
