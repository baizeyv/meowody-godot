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
    # Chained startup tasks dispatch via call_deferred at the end of a frame.
    await process_frame
    while create.disabled and Time.get_ticks_msec() < deadline:
        await process_frame
    check(not create.disabled, "Native task timed out")

func toggle_text_image(toggle, hover: bool):
    var motion = InputEventMouseMotion.new()
    motion.position = toggle.get_global_rect().get_center() if hover else Vector2(2, 2)
    motion.global_position = motion.position
    root.push_input(motion)
    await process_frame
    await RenderingServer.frame_post_draw
    var rect = toggle.get_global_rect()
    return root.get_texture().get_image().get_region(Rect2i(rect.position + Vector2(50, 8), rect.size - Vector2(64, 16)))

func check_window_layout(view, size: Vector2i):
    root.size = size
    for frame in range(12):
        await process_frame
    check(view.size.is_equal_approx(Vector2(size)), "UI viewport remained fixed when resizing the window")
    var columns = view.find_child("ContentColumns", true, false)
    check(columns.is_vertical() == (size.x < 1280), "Content did not switch between columns and stacked layout")
    check(view.find_child("Sidebar", true, false).visible == (size.x >= 1500), "Sidebar did not adapt to window width")
    for node_name in ["ProjectName", "FrameworkRef", "Destination", "EnginePath", "BuildLog", "create", "rebuild"]:
        var rect = view.find_child(node_name, true, false).get_global_rect()
        check(rect.position.x >= -1 and rect.end.x <= size.x + 1, node_name + " overflows the window horizontally")
    var workbench = view.find_child("WorkbenchPanel", true, false)
    var form = columns.get_child(0)
    check(workbench.position.y > form.position.y if size.x < 1280 else workbench.position.x > form.position.x, "Workbench position did not follow the responsive layout")
    var scroll = view.find_child("ContentScroll", true, false)
    scroll.scroll_vertical = 100000
    for frame in range(6):
        await process_frame
    var last_action = view.find_child("OpenProject", true, false).get_global_rect()
    check(last_action.position.y >= 0 and last_action.end.y <= size.y + 1, "Bottom actions are not reachable by vertical scrolling")
    scroll.scroll_vertical = 0
    for frame in range(6):
        await process_frame
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://creator-resize-%dx%d.png" % [size.x, size.y])

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
    var framework_ref = view.find_child("FrameworkRef", true, false)
    check(framework_ref is OptionButton and framework_ref.item_count > 0, "Framework references are not listed in a dropdown")
    check(str(framework_ref.get_selected_metadata()).begins_with("refs/"), "Framework branch or tag was not queried from the repository")
    var build_log = view.find_child("BuildLog", true, false)
    build_log.text = "VeryLongUnbrokenBuildOutput/".repeat(100)
    await process_frame
    check(build_log.get_line_wrap_count(0) > 0 and build_log.scroll_horizontal == 0, "Build output must wrap without horizontal scrolling")
    if DisplayServer.get_name() != "headless":
        for window_size in [Vector2i(1440, 900), Vector2i(1280, 720), Vector2i(1024, 768), Vector2i(900, 640), Vector2i(1920, 1080)]:
            await check_window_layout(view, window_size)
        check(view.find_child("WorkbenchPanel", true, false).size.y > 600, "Workbench did not expand with window height")
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
    for state in ["normal", "hover", "pressed", "hover_pressed", "disabled"]:
        for side in [SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM]:
            check(local_mode.get_theme_stylebox(state).get_content_margin(side) == 14, "Checkbox state margins are inconsistent")
    if DisplayServer.get_name() != "headless":
        for pressed in [false, true]:
            local_mode.button_pressed = pressed
            await process_frame
            var before = await toggle_text_image(local_mode, false)
            var after = await toggle_text_image(local_mode, true)
            check(before.get_data() == after.get_data(), "Checkbox text moved or changed on hover")
        local_mode.button_pressed = true
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
        view.browse(false)
        await process_frame
        var picker = view.find_child("LocationPicker", true, false)
        var scrim = view.find_child("PickerScrim", true, false)
        check(picker.visible and picker.exclusive and picker.transient and picker.is_embedded(), "Folder selection is not an embedded modal")
        check(scrim.visible and not picker.use_native_dialog, "Modal backdrop or custom theme is missing")
        check(picker.size.x < root.size.x * 0.8, "Folder modal is too wide")
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://creator-folder-dialog.png")
        root.size = Vector2i(900, 640)
        for frame in range(12):
            await process_frame
        check(picker.position.x >= 0 and picker.position.y >= 0 and (picker.position + picker.size).x <= root.size.x and (picker.position + picker.size).y <= root.size.y, "Open folder modal escaped the resized window")
        root.size = Vector2i(1920, 1080)
        for frame in range(12):
            await process_frame
        picker.hide()
        await process_frame
        check(not scrim.visible, "Modal backdrop was not removed on cancel")
        var before_cancel = view.find_child("Destination", true, false).text
        check(before_cancel == "D:/Projects/MyGame", "Canceling the folder modal changed the destination")
        view.find_child("FrameworkRef", true, false).show_popup()
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://creator-framework-refs.png")
        view.find_child("FrameworkRef", true, false).get_popup().hide()
        var dropdown = view.find_child("LocalEngines", true, false)
        dropdown.show_popup()
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://creator-engines.png")
        dropdown.get_popup().hide()
    root.remove_child(view)
    view.free()
    print("[CreatorUI] failures=", failures)
    quit(0 if failures == 0 else 1)
