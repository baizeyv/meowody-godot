@tool
extends EditorPlugin
const Settings = preload("settings.gd")
const Builder = preload("builder.gd")
const ResourcePanel = preload("panel.gd")
const Exporter = preload("export_plugin.gd")
var panel: Control
var exporter: EditorExportPlugin
var build_pid := -1
var timer: Timer
const RESULT := "res://.godot/meowody_resources/result.json"

func _enter_tree() -> void:
	exporter = Exporter.new()
	add_export_plugin(exporter)
	var args := OS.get_cmdline_user_args()
	if "--resource-analyze" in args:
		call_deferred("_worker", args)
		return
	if FileAccess.file_exists("res://.resource_build_context.json"): return
	panel = ResourcePanel.new()
	panel.manager = self
	panel.name = "Resources"
	EditorInterface.get_editor_main_screen().add_child(panel)
	panel.hide()
	timer = Timer.new()
	timer.wait_time = 0.5
	timer.timeout.connect(_poll_build)
	add_child(timer)
	if "--resource-editor-test" in args: call_deferred("_editor_test")

func _has_main_screen() -> bool:
	return true

func _get_plugin_name() -> String:
	return "Resources"

func _make_visible(visible: bool) -> void:
	if panel != null: panel.visible = visible

func _save_external_data() -> void:
	if panel != null and FileAccess.file_exists(Settings.PATH): panel.save()

func _exit_tree() -> void:
	cancel_build()
	remove_export_plugin(exporter)
	if panel != null: panel.queue_free()
	if timer != null: timer.queue_free()

func start_build(profile: String, platform: String, analyze_only: bool) -> void:
	if build_pid != -1:
		panel.output.text = "A resource build is already running"
		return
	DirAccess.remove_absolute(RESULT)
	var args := PackedStringArray([ProjectSettings.globalize_path("res://addons/meowody_resources/build.py"), "--project", ProjectSettings.globalize_path("res://"), "--godot", OS.get_executable_path(), "--profile", profile, "--platform", platform])
	if analyze_only: args.append("--analyze")
	build_pid = OS.create_process(str(ProjectSettings.get_setting("meowody/resources/python", "python")), args)
	panel.output.text = "Building content in an isolated project…" if build_pid != -1 else "Could not start Python; configure meowody/resources/python in Project Settings"
	if build_pid != -1: timer.start()

func _poll_build() -> void:
	if build_pid == -1 or OS.is_process_running(build_pid): return
	build_pid = -1
	timer.stop()
	var result := Settings.read_json(RESULT)
	panel.output.text = JSON.stringify(result, "\t") if not result.is_empty() else "Build stopped before producing a result; inspect the selected profile's build.log"
	EditorInterface.get_resource_filesystem().scan()

func cancel_build() -> void:
	# The coordinator owns its child editor. A cancellation marker lets it stop the child safely.
	if build_pid != -1:
		Settings.save_json("res://.godot/meowody_resources/cancel.json", {"cancel": true})
		if panel != null: panel.output.text = "Cancelling content build…"

func _worker(args: PackedStringArray) -> void:
	while EditorInterface.get_resource_filesystem().is_scanning(): await get_tree().process_frame
	await get_tree().create_timer(1.0).timeout
	while EditorInterface.get_resource_filesystem().is_scanning(): await get_tree().process_frame
	for sibling in get_parent().get_children():
		var script = sibling.get_script()
		if script != null and script.resource_path.ends_with("/meowody_atlas/plugin.gd"):
			while sibling.busy: await get_tree().process_frame
			var atlas_error: String = sibling.prepare_export()
			if not atlas_error.is_empty():
				Settings.save_json(args[args.find("--resource-analyze") + 1], {"errors": [atlas_error]})
				get_tree().quit(1)
				return
	var settings := Settings.load_settings()
	var result := Builder.analyze(settings)
	var target := args[args.find("--resource-analyze") + 1]
	var error := Settings.save_json(target, result)
	get_tree().quit(0 if error == OK and result.get("errors", []).is_empty() else 1)

func _editor_test() -> void:
	await get_tree().create_timer(1.0).timeout
	while EditorInterface.get_resource_filesystem().is_scanning(): await get_tree().process_frame
	await load("res://tests/meowody_resources/editor_test.gd").new().run(self)
