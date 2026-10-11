extends Control
var grid: GridContainer
var status: Label
func native(type: String) -> Control:
	return ClassDB.instantiate(type)
func card(title: String, node: Control, extent := Vector2(300, 140)) -> void:
	var box := VBoxContainer.new()
	box.custom_minimum_size.x = 320
	box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var label := Label.new()
	label.text = title
	box.add_child(label)
	node.custom_minimum_size = extent
	node.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	box.add_child(node)
	grid.add_child(box)
func _ready() -> void:
	var backdrop := ColorRect.new()
	backdrop.color = Color("141b29")
	backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(backdrop)
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side in ["left","top","right","bottom"]: margin.add_theme_constant_override("margin_" + side, 24)
	add_child(margin)
	var column := VBoxContainer.new()
	margin.add_child(column)
	var heading := Label.new()
	heading.text = "MEOWODY / COMMON NODES"
	heading.add_theme_font_size_override("font_size", 26)
	column.add_child(heading)
	status = Label.new()
	status.text = "Scroll lists, hold buttons, drag the card and try gestures."
	column.add_child(status)
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(scroll)
	grid = GridContainer.new()
	grid.columns = 3
	grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	grid.add_theme_constant_override("h_separation",24)
	grid.add_theme_constant_override("v_separation",20)
	scroll.add_child(grid)
	for type in ["VirtualList", "SnapScrollContainer", "LoopScrollContainer"]:
		var list := native(type)
		list.item_count = 10000 if type == "VirtualList" else 7
		list.item_extent = 42 if type == "VirtualList" else 130
		list.direction = 0 if type == "VirtualList" else 1
		list.bind_item.connect(func(cell, index):
			if cell is Label: cell.text = "# %05d" % index)
		card(type + (" / 10,000 rows" if type == "VirtualList" else " / swipe or wheel"), list, Vector2(300,180))
	var segments := native("SegmentProgressBar")
	segments.segments = 8
	segments.value = 5.5
	card("SegmentProgressBar / partial fill", segments, Vector2(300,42))
	var arc := native("ArcProgress")
	arc.value = 0.72
	card("ArcProgress / round caps", arc)
	var number := native("NumberLabel")
	number.value = 12500
	number.prefix = "$ "
	number.add_theme_font_size_override("font_size",32)
	card("NumberLabel / click to add", number, Vector2(300,60))
	number.mouse_filter = Control.MOUSE_FILTER_STOP
	number.gui_input.connect(func(event):
		if event is InputEventMouseButton and event.pressed: number.value += 1750)
	var auto := native("AutoSizeLabel")
	auto.text = "自动字号 / Adaptive multilingual text"
	auto.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	card("AutoSizeLabel / resize the window", auto, Vector2(300,80))
	var gradient := native("GradientRect")
	gradient.gradient = Gradient.new()
	gradient.gradient.colors = PackedColorArray([Color("78f5c7"), Color("5566ee")])
	gradient.gradient_mode = 2
	card("GradientRect / conic", gradient)
	var shape := native("ShapeRect")
	shape.corner_radii = Vector4(30,8,30,8)
	shape.border_width = 2
	shape.border_color = Color("78f5c7")
	card("ShapeRect / border + shadow", shape)
	var long_button := native("LongPressButton")
	long_button.text = "Hold to confirm"
	long_button.long_press_progress.connect(func(value): status.text = "Hold: %d%%" % (value*100))
	long_button.long_pressed.connect(func(): status.text = "Confirmed")
	card("LongPressButton", long_button, Vector2(300,60))
	var repeat_button := native("RepeatButton")
	repeat_button.text = "Hold to accelerate"
	repeat_button.repeated.connect(func(count): status.text = "Repeat action: " + str(count))
	card("RepeatButton", repeat_button, Vector2(300,60))
	var gestures := native("GestureArea")
	gestures.tap.connect(func(position): status.text = "Tap: " + str(position))
	gestures.pan.connect(func(delta): status.text = "Pan: " + str(delta))
	gestures.pinch.connect(func(scale): status.text = "Pinch: " + str(scale))
	gestures.rotate.connect(func(angle): status.text = "Rotate radians: " + str(angle))
	card("GestureArea / drag or touch", gestures, Vector2(300,60))
	var source := native("DragArea")
	source.drag_type = "card"
	source.data = "Example card"
	var source_label := Label.new()
	source_label.text = "DRAG THIS CARD →"
	source_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	source.add_child(source_label)
	card("DragArea / source stays in place", source, Vector2(300,60))
	var target := native("DropArea")
	target.allowed_types = PackedStringArray(["card"])
	target.dropped.connect(func(data): status.text = "Dropped: " + str(data))
	var target_label := Label.new()
	target_label.text = "DROP CARD HERE"
	target_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	target.add_child(target_label)
	card("DropArea / type filter + highlight", target, Vector2(300,60))
