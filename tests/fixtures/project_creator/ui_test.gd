extends SceneTree

var failures := 0

func check(value: bool, message: String):
    if not value:
        failures += 1
        push_error(message)

func _initialize():
    call_deferred("run")

func wait_idle(view):
    await process_frame
    var deadline = Time.get_ticks_msec() + 60000
    var create = view.find_child("create", true, false)
    while create.disabled and Time.get_ticks_msec() < deadline:
        await process_frame
    check(not create.disabled, "Native task timed out")

func run():
    var view = load("res://project_creator/main.tscn").instantiate()
    root.add_child(view)
    await process_frame
    var name_field = view.find_child("ProjectName", true, false)
    var destination = view.find_child("Destination", true, false)
    var create = view.find_child("create", true, false)
    var status = view.find_child("Status", true, false)
    check(name_field != null and create != null, "Native creator UI did not load")
    if name_field == null or create == null:
        quit(1)
        return
    await wait_idle(view)
    check(view.get_theme_color("font_color", "Label").is_equal_approx(Color("cdd6f4")), "Catppuccin Mocha text color was not applied")
    var engines = view.find_child("LocalEngines", true, false)
    check(engines.item_count > 1, "Automatic search did not find the running Godot editor")
    check(engines.selected == 0 and view.find_child("EnginePath", true, false).text.is_empty(), "A new installation must wait for manual engine selection")
    var selected_engine = -1
    for index in range(1, engines.item_count):
        if str(engines.get_item_metadata(index)).replace("\\", "/").to_lower() == OS.get_executable_path().replace("\\", "/").to_lower():
            selected_engine = index
    check(selected_engine > 0, "Running editor was not included in the compatible engine dropdown")
    if selected_engine > 0:
        engines.select(selected_engine)
        engines.item_selected.emit(selected_engine)
    check(not view.find_child("EnginePath", true, false).editable, "Engine path must be selected from verified entries")
    check(not ClassDB.class_exists("YatzyGame") and not ClassDB.class_exists("TestNode"), "Old business classes remain registered")
    var local_mode = view.find_child("UseLocalEngine", true, false)
    local_mode.button_pressed = false
    await wait_idle(view)
    var versions = view.find_child("EngineVersion", true, false)
    check(versions.item_count > 0 and versions.selected == 0, "Online latest stable release was not selected automatically")
    check(view.find_child("EngineVersion", true, false).is_visible_in_tree(), "Download engine fields did not appear")
    check(not view.find_child("EnginePath", true, false).is_visible_in_tree(), "Local engine fields should be hidden in download mode")
    local_mode.button_pressed = true
    check(view.find_child("EnginePath", true, false).is_visible_in_tree(), "Local engine fields did not reappear")
    name_field.text = "Creator Regression"
    destination.text = ""
    view.run_operation("create")
    check(not create.disabled and not status.text.is_empty(), "Missing input should fail without starting a task")
    var occupied = ProjectSettings.globalize_path("user://occupied")
    DirAccess.make_dir_recursive_absolute(occupied)
    var marker = FileAccess.open(occupied.path_join("keep.txt"), FileAccess.WRITE)
    marker.store_string("keep")
    marker.close()
    destination.text = occupied
    view.run_operation("create")
    check(create.disabled and not name_field.editable, "Fields must be locked while the worker runs")
    # App routes close requests through ApplicationRequest; the creator consumes it.
    root.propagate_notification(Node.NOTIFICATION_WM_CLOSE_REQUEST)
    check(status.text.contains("任务"), "Busy creator did not intercept close")
    var deadline = Time.get_ticks_msec() + 20000
    while create.disabled and Time.get_ticks_msec() < deadline:
        await process_frame
    check(not create.disabled and name_field.editable, "Failed worker did not restore controls")
    check(status.text.contains("未完成") and status.text.contains("empty"), "Native directory protection failure was not shown")
    check(FileAccess.get_file_as_string(occupied.path_join("keep.txt")) == "keep", "Nonempty destination was changed")
    # Let SaveCenter's deferred phase persist preferences, then reload the view.
    for i in range(5):
        await process_frame
    root.remove_child(view)
    view.free()
    view = load("res://project_creator/main.tscn").instantiate()
    root.add_child(view)
    await wait_idle(view)
    check(view.find_child("ProjectName", true, false).text == "Creator Regression", "SaveCenter preferences were not restored")
    check(view.find_child("Destination", true, false).text == occupied, "Destination preference was not restored")
    if DisplayServer.get_name() != "headless":
        view.find_child("ProjectName", true, false).text = "MyGame"
        view.find_child("Destination", true, false).text = "D:/Projects/MyGame"
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://creator-ui.png")
        var dropdown = view.find_child("LocalEngines", true, false)
        dropdown.show_popup()
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://creator-engines.png")
        dropdown.get_popup().hide()
    root.remove_child(view)
    view.free()
    print("[CreatorUI] failures=", failures)
    quit(0 if failures == 0 else 1)
