@tool
extends Control

signal edited(before: Dictionary)
signal selected(axis: int, segment: int)
var sprite: Resource
var region_mode := false
var axis := 0
var selected_cut := -1
var zoom := 1.0
var pan := Vector2.ZERO
var dragging := false
var panning := false
var region_start := Vector2.ZERO
var region_end := Vector2.ZERO
var before := {}
const FIELDS := ["resource_name", "region", "x_cuts", "y_cuts", "x_modes", "y_modes", "x_weights", "y_weights"]

func _ready() -> void:
	custom_minimum_size = Vector2(340, 240)
	size_flags_horizontal = Control.SIZE_EXPAND_FILL
	size_flags_vertical = Control.SIZE_EXPAND_FILL
	focus_mode = Control.FOCUS_ALL
	clip_contents = true
	resized.connect(queue_redraw)

func snapshot() -> Dictionary:
	var data := {}
	for field in FIELDS:
		data[field] = sprite.get(field)
	return data

# Preserve other segments when inserting/removing a cut. A split inherits its mode;
# merged segments retain the left mode and combine their allocation weights.
static func edit_cut(data: Resource, direction: int, index: int, value: int = -1) -> void:
	var prefix := "x_" if direction == 0 else "y_"
	var cuts: PackedInt32Array = data.get(prefix + "cuts")
	var modes: PackedInt32Array = data.get(prefix + "modes")
	var weights: PackedFloat32Array = data.get(prefix + "weights")
	if modes.size() != cuts.size() + 1:
		modes.resize(cuts.size() + 1)
		modes.fill(1)
		if not cuts.is_empty():
			modes[0] = 0
			modes[-1] = 0
	if weights.size() != cuts.size() + 1:
		weights.resize(cuts.size() + 1)
		weights.fill(1)
	if value >= 0:
		var start := cuts[index - 1] if index > 0 else 0
		var end: int = cuts[index] if index < cuts.size() else data.get_effective_region().size[direction]
		var left := weights[index] * float(value - start) / (end - start)
		weights.insert(index + 1, weights[index] - left)
		weights[index] = left
		modes.insert(index + 1, modes[index])
		cuts.insert(index, value)
	else:
		weights[index] += weights[index + 1]
		weights.remove_at(index + 1)
		modes.remove_at(index + 1)
		cuts.remove_at(index)
	data.set(prefix + "cuts", cuts)
	data.set(prefix + "modes", modes)
	data.set(prefix + "weights", weights)

func source_rect() -> Rect2:
	if sprite == null or sprite.texture == null:
		return Rect2()
	return Rect2(Vector2.ZERO, sprite.texture.get_size()) if region_mode else Rect2(sprite.get_effective_region())

func origin() -> Vector2:
	return (size - source_rect().size * zoom) * 0.5 + pan

func fit() -> void:
	if source_rect().has_area():
		zoom = clampf(minf((size.x - 48) / source_rect().size.x, (size.y - 48) / source_rect().size.y), 0.05, 32)
	pan = Vector2.ZERO
	queue_redraw()

func texture_point(position: Vector2) -> Vector2:
	return (position - origin()) / zoom + source_rect().position

func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color("20242d"))
	if sprite == null or sprite.texture == null:
		return
	var src := source_rect()
	if not src.has_area():
		return
	var box := Rect2(origin(), src.size * zoom)
	var square := 16.0
	var visible := box.intersection(Rect2(Vector2.ZERO, size))
	for y in range(int(visible.position.y / square), ceili(visible.end.y / square)):
		for x in range(int(visible.position.x / square), ceili(visible.end.x / square)):
			draw_rect(Rect2(Vector2(x, y) * square, Vector2.ONE * square).intersection(box), Color("50545c") if (x + y) % 2 else Color("3a3e47"))
	draw_texture_rect_region(sprite.texture, box, src)
	if region_mode:
		var region: Rect2 = Rect2(sprite.get_effective_region())
		if dragging:
			region = Rect2(region_start, region_end - region_start).abs()
		draw_rect(Rect2(origin() + (region.position - src.position) * zoom, region.size * zoom), Color("65e3c1"), false, 2)
	else:
		for direction in range(2):
			var cuts: PackedInt32Array = sprite.x_cuts if direction == 0 else sprite.y_cuts
			for i in range(cuts.size()):
				var a := origin()
				var b := origin() + src.size * zoom
				if direction == 0:
					a.x += cuts[i] * zoom
					b.x = a.x
				else:
					a.y += cuts[i] * zoom
					b.y = a.y
				draw_line(a, b, Color("ffcb70") if axis == direction and selected_cut == i else Color("65e3c1"), 2)
	draw_rect(box, Color("8a95a8"), false)

func _gui_input(event: InputEvent) -> void:
	if sprite == null or sprite.texture == null:
		return
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_MIDDLE:
			panning = event.pressed
			accept_event()
		elif event.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
			var old := zoom
			zoom = clampf(zoom * (1.15 if event.button_index == MOUSE_BUTTON_WHEEL_UP else 1.0 / 1.15), 0.05, 32)
			pan = event.position - size * 0.5 - (event.position - size * 0.5 - pan) * zoom / old
			queue_redraw()
			accept_event()
		elif event.button_index == MOUSE_BUTTON_LEFT:
			grab_focus()
			if event.pressed:
				before = snapshot()
				if region_mode:
					region_start = texture_point(event.position).round().clamp(Vector2.ZERO, sprite.texture.get_size())
					region_end = region_start
					dragging = true
				else:
					var point := texture_point(event.position) - Vector2(sprite.get_effective_region().position)
					var cuts: PackedInt32Array = sprite.x_cuts if axis == 0 else sprite.y_cuts
					selected_cut = -1
					for i in range(cuts.size()):
						if absf(cuts[i] - point[axis]) * zoom <= 7:
							selected_cut = i
							break
					if event.double_click and selected_cut < 0:
						var cut := roundi(point[axis])
						if cut > 0 and cut < sprite.get_effective_region().size[axis] and cuts.size() < 64:
							edit_cut(sprite, axis, cuts.bsearch(cut), cut)
							cuts = sprite.x_cuts if axis == 0 else sprite.y_cuts
							edited.emit(before)
							selected_cut = cuts.find(cut)
					dragging = selected_cut >= 0 and not event.double_click
					var segment := 0
					while segment < cuts.size() and point[axis] > cuts[segment]:
						segment += 1
					selected.emit(axis, selected_cut if selected_cut >= 0 else segment)
			else:
				if dragging:
					if region_mode:
						var region := Rect2i(Rect2(region_start, region_end - region_start).abs())
						if region.has_area():
							sprite.region = region
							for field in ["x_cuts", "y_cuts", "x_modes", "y_modes"]:
								sprite.set(field, PackedInt32Array())
							for field in ["x_weights", "y_weights"]:
								sprite.set(field, PackedFloat32Array())
					edited.emit(before)
				dragging = false
			queue_redraw()
			accept_event()
	elif event is InputEventMouseMotion:
		if panning:
			pan += event.relative
		elif dragging:
			if region_mode:
				region_end = texture_point(event.position).round().clamp(Vector2.ZERO, sprite.texture.get_size())
			else:
				var cuts: PackedInt32Array = sprite.x_cuts if axis == 0 else sprite.y_cuts
				var lower := cuts[selected_cut - 1] + 1 if selected_cut > 0 else 1
				var upper: int = cuts[selected_cut + 1] - 1 if selected_cut + 1 < cuts.size() else sprite.get_effective_region().size[axis] - 1
				cuts[selected_cut] = clampi(roundi((texture_point(event.position) - Vector2(sprite.get_effective_region().position))[axis]), lower, upper)
				sprite.set("x_cuts" if axis == 0 else "y_cuts", cuts)
		queue_redraw()
		accept_event()
