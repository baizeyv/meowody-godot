@tool
extends EditorPlugin

var panel: TabContainer
var capture: EditorDebuggerPlugin
var listing: Tree
var details: RichTextLabel
var search: LineEdit
var tag_filter: LineEdit
var levels: Array[CheckBox] = []
var collapse: CheckBox
var paused: CheckBox
var follow: CheckBox
var count: Label
var reports: OptionButton
var report_text: RichTextLabel
var records: Array[Dictionary] = []
var rows: Array = []
var retained_bytes := 0
var dropped := 0
var serial := 0
var dirty := true
var elapsed := 0.0
const LEVELS := ["Debug", "Info", "Warn", "Error", "Fatal"]

func _native(method: StringName, args: Array = []) -> Variant:
    if not ClassDB.class_exists("DebugEditorBackend"):
        return []
    var backend = ClassDB.instantiate("DebugEditorBackend")
    return backend.callv(method, args)

func _enter_tree() -> void:
    panel = TabContainer.new()
    panel.custom_minimum_size = Vector2(500, 320)
    var console := VBoxContainer.new()
    console.name = "Console"
    panel.add_child(console)
    var bar := HBoxContainer.new()
    console.add_child(bar)
    for level in LEVELS:
        var toggle := CheckBox.new()
        toggle.text = level
        toggle.button_pressed = true
        toggle.toggled.connect(func(_value): dirty = true)
        bar.add_child(toggle)
        levels.append(toggle)
    _button(bar, "Clear", _clear)
    _button(bar, "Copy detail", func(): DisplayServer.clipboard_set(details.get_parsed_text()))
    _button(bar, "Export filtered", _export)
    var filters := HBoxContainer.new()
    console.add_child(filters)
    search = LineEdit.new()
    search.placeholder_text = "Search text / source"
    search.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    search.text_changed.connect(func(_value): dirty = true)
    filters.add_child(search)
    tag_filter = LineEdit.new()
    tag_filter.placeholder_text = "Exact Tag"
    tag_filter.text_changed.connect(func(_value): dirty = true)
    filters.add_child(tag_filter)
    collapse = _toggle(filters, "Collapse")
    paused = _toggle(filters, "Pause display")
    follow = _toggle(filters, "Auto scroll")
    follow.button_pressed = true
    count = Label.new()
    console.add_child(count)
    listing = Tree.new()
    listing.columns = 4
    listing.hide_root = true
    listing.column_titles_visible = true
    listing.custom_minimum_size.y = 140
    listing.size_flags_vertical = Control.SIZE_EXPAND_FILL
    for i in range(4):
        listing.set_column_title(i, ["Level / Count", "Time / Source", "Tag", "Message"][i])
        if i < 3:
            listing.set_column_expand(i, false)
            listing.set_column_custom_minimum_width(i, 180 if i == 1 else 100)
    listing.item_selected.connect(_selected)
    console.add_child(listing)
    details = RichTextLabel.new()
    details.selection_enabled = true
    details.custom_minimum_size.y = 140
    details.size_flags_vertical = Control.SIZE_EXPAND_FILL
    console.add_child(details)
    var crashes := VBoxContainer.new()
    crashes.name = "Crash reports"
    panel.add_child(crashes)
    var crash_bar := HBoxContainer.new()
    crashes.add_child(crash_bar)
    reports = OptionButton.new()
    reports.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    reports.item_selected.connect(_show_report)
    crash_bar.add_child(reports)
    _button(crash_bar, "Refresh", _refresh_reports)
    _button(crash_bar, "Open folder", func(): OS.shell_open(ProjectSettings.globalize_path("user://logs")))
    report_text = RichTextLabel.new()
    report_text.selection_enabled = true
    report_text.size_flags_vertical = Control.SIZE_EXPAND_FILL
    crashes.add_child(report_text)
    _refresh_reports()
    panel.tab_changed.connect(func(index: int):
        if index == 1:
            _refresh_reports())
    add_control_to_bottom_panel(panel, "Meowody Console")
    capture = preload("capture.gd").new()
    capture.records_received.connect(_receive)
    add_debugger_plugin(capture)
    set_process(true)
    if OS.get_cmdline_user_args().has("--debug-console-editor-test"):
        call_deferred("_run_test")

func _exit_tree() -> void:
    set_process(false)
    remove_debugger_plugin(capture)
    capture = null
    remove_control_from_bottom_panel(panel)
    panel.free()

func _button(parent: Control, title: String, callback: Callable) -> void:
    var button := Button.new()
    button.text = title
    button.pressed.connect(callback)
    parent.add_child(button)

func _toggle(parent: Control, title: String) -> CheckBox:
    var toggle := CheckBox.new()
    toggle.text = title
    toggle.toggled.connect(func(_value): dirty = true)
    parent.add_child(toggle)
    return toggle

func _receive(batch: Array, source: String) -> void:
    for data in batch:
        if not data is Array or data.size() < 6 or not data[2] is int or data[2] < 0 or data[2] > 4:
            continue
        if not data[3] is String or not data[4] is String or not data[5] is PackedStringArray:
            continue
        serial += 1
        var text: String = data[4].left(16384)
        var spans: Array = _native("spans", [text, data[6] if data.size() > 6 else false])
        if spans.is_empty():
            spans = [{"text": text, "color": Color.WHITE}]
        var plain := ""
        for span in spans:
            plain += span.text
        var record := {"id": serial, "time": data[1], "level": data[2], "tag": data[3], "text": plain,
            "source": source, "stack": data[5].slice(0, 24), "spans": spans}
        record.bytes = text.to_utf8_buffer().size() + str(record.stack).to_utf8_buffer().size()
        records.append(record)
        retained_bytes += record.bytes
        while records.size() > 2000 or retained_bytes > 4 * 1024 * 1024:
            retained_bytes -= records.pop_front().bytes
            dropped += 1
        dirty = true

func _process(delta: float) -> void:
    elapsed += delta
    if elapsed < 0.25:
        return
    elapsed = 0.0
    _receive(_native("drain"), "Editor")
    if dirty and not paused.button_pressed:
        _redraw()

func _redraw() -> void:
    var selected_id: int = listing.get_selected().get_metadata(1) if listing.get_selected() else -1
    rows.clear()
    var groups := {}
    for record in records:
        if not levels[record.level].button_pressed:
            continue
        if not tag_filter.text.is_empty() and tag_filter.text.nocasecmp_to(record.tag) != 0:
            continue
        if not search.text.is_empty() and record.text.findn(search.text) < 0 and record.source.findn(search.text) < 0:
            continue
        var key := str([record.level, record.tag, record.source, record.text])
        if collapse.button_pressed and groups.has(key):
            rows[groups[key]].append(record)
        else:
            groups[key] = rows.size()
            rows.append([record])
    listing.clear()
    var root := listing.create_item()
    var last: TreeItem
    for i in range(rows.size()):
        var record: Dictionary = rows[i].back()
        var item := listing.create_item(root)
        item.set_metadata(0, i)
        item.set_metadata(1, record.id)
        item.set_text(0, "%s x%s" % [LEVELS[record.level], rows[i].size()])
        item.set_text(1, "%.3f %s" % [record.time, record.source])
        item.set_text(2, record.tag)
        item.set_cell_mode(3, TreeItem.CELL_MODE_CUSTOM)
        item.set_custom_draw_callback(3, _draw_message)
        item.set_tooltip_text(3, record.text)
        if record.id == selected_id:
            item.select(0)
        last = item
    if follow.button_pressed and last:
        listing.scroll_to_item(last)
    count.text = "%s rows / %s retained / %s dropped" % [rows.size(), records.size(), dropped]
    dirty = false

func _draw_message(item: TreeItem, rect: Rect2) -> void:
    var spans: Array = rows[item.get_metadata(0)].back().spans
    var font := listing.get_theme_font("font")
    var size := listing.get_theme_font_size("font_size")
    var header := font.get_height(size) + listing.get_theme_stylebox("title_button_normal").get_minimum_size().y
    if rect.position.y < header or rect.end.y > listing.size.y:
        return
    var position := rect.position + Vector2(0, (rect.size.y - font.get_height(size)) / 2 + font.get_ascent(size))
    var width := rect.size.x
    for span in spans:
        var line: String = span.text.get_slice("\n", 0)
        listing.draw_string(font, position, line, HORIZONTAL_ALIGNMENT_LEFT, width, size, span.color)
        var used := font.get_string_size(line, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x
        width -= used
        position.x += used
        if width <= 0 or span.text.contains("\n"):
            break

func _selected() -> void:
    if listing.get_selected() == null:
        return
    details.clear()
    for record in rows[listing.get_selected().get_metadata(0)]:
        details.add_text("#%s %s [%s]\n" % [record.id, record.source, record.tag])
        for span in record.spans:
            details.push_color(span.color)
            details.add_text(span.text)
            details.pop()
        details.add_text("\n\nNative stack:\n" + "\n".join(record.stack) + "\n---\n")
        if details.get_total_character_count() > 512 * 1024:
            break

func _clear() -> void:
    records.clear()
    retained_bytes = 0
    dropped = 0
    details.clear()
    dirty = true

func _export() -> void:
    DirAccess.make_dir_recursive_absolute("user://logs")
    var path := "user://logs/console_%s.txt" % Time.get_ticks_usec()
    var file := FileAccess.open(path, FileAccess.WRITE)
    if file == null:
        details.text = "Export failed: " + error_string(FileAccess.get_open_error())
        return
    for row in rows:
        for record in row:
            file.store_string("%s [%s] %s\n%s\n\n" % [record.source, record.tag, record.text, "\n".join(record.stack)])
    details.text = "Exported: " + ProjectSettings.globalize_path(path)

func _refresh_reports() -> void:
    reports.clear()
    var directory := DirAccess.open("user://logs")
    if directory:
        var files := directory.get_files()
        files.sort()
        files.reverse()
        for name in files:
            if name.begins_with("crash_") and name.ends_with(".log"):
                reports.add_item(name)
    if reports.item_count:
        _show_report(0)
    else:
        report_text.text = "No native crash reports. Windows debug builds write reports to user://logs."

func _show_report(index: int) -> void:
    var file := FileAccess.open("user://logs/" + reports.get_item_text(index), FileAccess.READ)
    report_text.text = file.get_buffer(mini(file.get_length(), 1024 * 1024)).get_string_from_utf8() if file else "Cannot read report."

func _run_test() -> void:
    await get_tree().create_timer(1).timeout
    _native("write_test_log")
    _receive(_native("drain"), "Editor")
    capture._capture("meowody_log:records", [[1, 1.0, 1, "RemoteTest", "[color=red]remote rich[/color]", PackedStringArray(["remote frame"]), true]], 0)
    _redraw()
    var local := false
    var remote := false
    for record in records:
        local = local or record.tag == "EditorTest"
        remote = remote or (record.tag == "RemoteTest" and record.text == "remote rich")
    var panel_id := panel.get_instance_id()
    var status := GDExtensionManager.reload_extension("res://bin/meowody.gdextension")
    GDExtensionManager.emit_signal("extensions_reloaded")
    await get_tree().create_timer(1).timeout
    var ok := local and remote and status == GDExtensionManager.LOAD_STATUS_OK and panel.get_instance_id() == panel_id
    print("[DebugEditorTest] local=", local, " remote=", remote, " reload=", status, " failures=", 0 if ok else 1)
    get_tree().quit(0 if ok else 1)
