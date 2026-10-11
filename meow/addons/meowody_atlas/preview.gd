@tool
extends Control

var texture: Texture2D
var entries := {}
var page := 0
var zoom := 1.0
var pan := Vector2.ZERO
var show_regions := true
var image_rect := Rect2()

func _ready() -> void:
	clip_contents = true
	mouse_filter = Control.MOUSE_FILTER_STOP
	custom_minimum_size = Vector2(0, 230) * EditorInterface.get_editor_scale()
	resized.connect(queue_redraw)

func display(image: Image, sprites: Dictionary, page_index: int) -> void:
	texture = ImageTexture.create_from_image(image)
	entries = sprites
	page = page_index
	zoom = 1.0
	pan = Vector2.ZERO
	queue_redraw()

func _draw() -> void:
	var cell := 16.0 * EditorInterface.get_editor_scale()
	for y in range(ceili(size.y / cell)):
		for x in range(ceili(size.x / cell)):
			draw_rect(Rect2(x * cell, y * cell, cell, cell), Color("30343d") if (x + y) % 2 else Color("242730"))
	if texture == null:
		return
	var scale_factor := minf(size.x / texture.get_width(), size.y / texture.get_height()) * 0.92 * zoom
	var extent := texture.get_size() * scale_factor
	image_rect = Rect2((size - extent) * 0.5 + pan, extent)
	draw_texture_rect(texture, image_rect, false)
	if show_regions:
		for entry in entries.values():
			if entry.page == page:
				var r: Array = entry.region
				draw_rect(Rect2(image_rect.position + Vector2(r[0], r[1]) * scale_factor,
					Vector2(r[2], r[3]) * scale_factor), Color(0.4, 0.8, 1.0, 0.65), false)

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			zoom = minf(zoom * 1.2, 16.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			zoom = maxf(zoom / 1.2, 0.25)
		elif event.double_click:
			zoom = 1.0
			pan = Vector2.ZERO
		queue_redraw()
	elif event is InputEventMouseMotion and event.button_mask & MOUSE_BUTTON_MASK_MIDDLE:
		pan += event.relative
		queue_redraw()

func _get_tooltip(at_position: Vector2) -> String:
	if texture == null or image_rect.size.x == 0:
		return "Pack Preview to see the layout."
	var point := (at_position - image_rect.position) * texture.get_size() / image_rect.size
	for path in entries:
		var entry: Dictionary = entries[path]
		var r: Array = entry.region
		if entry.page == page and Rect2(r[0], r[1], r[2], r[3]).has_point(point):
			return "%s\n%d × %d\nScroll: zoom · Middle drag: pan · Double-click: fit" % [path, r[2], r[3]]
	return "Scroll: zoom · Middle drag: pan · Double-click: fit"
