extends SceneTree
var failures := 0
var counts := {}
var mouse_entered := false
func check(ok: bool, message: String) -> void:
	print("[NodesTest] ", "PASS " if ok else "FAIL ", message)
	if not ok: failures += 1
func node(type: String, at := Vector2.ZERO, extent := Vector2(300,200)) -> Control:
	var result: Control = ClassDB.instantiate(type)
	result.position = at
	result.size = extent
	root.add_child(result)
	return result
func tick() -> void:
	for i in range(4): await process_frame
func count(key: String) -> void:
	counts[key] = counts.get(key,0)+1
func _initialize() -> void:
	run.call_deferred()
func run() -> void:
	var packed: PackedScene = load("res://native_nodes.tscn")
	var restored := packed.instantiate()
	root.add_child(restored)
	await tick()
	check(restored.get_node("ShapeRect").get_class() == "ShapeRect" and restored.get_node("LoopScrollContainer").is_class("VirtualList"),"exported native inheritance and scene loading")
	restored.free()
	var list := node("VirtualList")
	list.item_count = 10000
	list.item_extent = 32
	list.bind_item.connect(func(_cell, _index): count("bind"))
	await tick()
	check(list.get_visible_node_count() < 20 and counts.get("bind",0)>0,"10,000 rows allocate only visible and preload cells")
	list.scroll_to_index(9999)
	await tick()
	check(list.get_item_control(9999) != null and list.get_visible_node_count()<30,"jump to last item reuses a bounded cell pool")
	list.item_count = 3
	list.item_extents = PackedFloat32Array([30,60,90])
	await tick()
	check(list.get_visible_node_count() == 3 and is_equal_approx(list.get_item_control(1).size.y,60),"shrinking data and variable item extents")
	list.columns = 2
	list.item_count = 100
	list.refresh()
	await tick()
	check(list.get_item_control(0) != null and list.get_item_control(1).position.x > 0,"grid lanes position reused cells")
	list.free()
	var snap := node("SnapScrollContainer")
	snap.direction = 1
	snap.item_count = 10
	snap.item_extent = 100
	snap.snap_duration = 0.05
	await tick()
	snap.set_current_index(3,false)
	check(snap.get_current_index() == 3,"snap selection API")
	snap.set_current_index(6,true)
	await create_timer(0.25).timeout
	check(snap.get_current_index() == 6,"animated snap reaches target")
	snap.free()
	var loop := node("LoopScrollContainer")
	loop.direction = 1
	loop.item_count = 2
	loop.item_extent = 80
	await tick()
	loop.scroll_horizontal = 0
	await tick()
	check(loop.scroll_horizontal > 0 and loop.get_visible_node_count() < 30,"loop recenters while retaining bounded cells for a short data set")
	loop.set_current_index(1,true)
	await create_timer(0.5).timeout
	check(loop.get_current_index()==1,"loop also supports snap selection")
	loop.free()
	var number := node("NumberLabel",Vector2.ZERO,Vector2(300,50))
	number.animate_change = false
	number.decimals = 2
	number.prefix = "$"
	number.value = 1234.5
	await tick()
	check(number.text == "$1,234.50","number grouping, decimals and prefix")
	number.animate_change = true
	number.duration = 0.05
	number.value = 2000
	await create_timer(0.15).timeout
	check(number.get_displayed_value()==2000,"number animation finishes at exact target")
	number.rolling = true
	number.value = 3500
	await create_timer(0.15).timeout
	check(number.text == "$3,500.00","rolling animation restores final text")
	number.free()
	var label := node("AutoSizeLabel",Vector2.ZERO,Vector2(180,48))
	label.text = "A reasonably long localized label"
	await tick()
	check(label.get_fitted_font_size()>=12 and label.get_fitted_font_size()<40,"automatic font sizing")
	label.free()
	var long_button := node("LongPressButton",Vector2.ZERO,Vector2(150,50))
	check(not long_button.is_processing(),"idle long press button skips frame processing")
	long_button.long_press_time = 0.03
	long_button.long_pressed.connect(func(): count("long"))
	long_button.emit_signal("button_down")
	await create_timer(0.08).timeout
	long_button.emit_signal("button_up")
	check(counts.get("long",0)==1,"long press fires once and releases")
	long_button.emit_signal("button_down")
	long_button.disabled = true
	await create_timer(0.08).timeout
	check(counts.get("long",0)==1,"disabled button cancels pending long press")
	long_button.free()
	var repeat := node("RepeatButton",Vector2.ZERO,Vector2(150,50))
	check(not repeat.is_processing(),"idle repeat button skips frame processing")
	repeat.initial_delay = 0.02
	repeat.repeat_interval = 0.02
	repeat.repeated.connect(func(_index): count("repeat"))
	repeat.emit_signal("button_down")
	await create_timer(0.12).timeout
	repeat.emit_signal("button_up")
	var total: int = counts.get("repeat",0)
	await create_timer(0.06).timeout
	check(total>=3 and total==counts.get("repeat",0),"repeat emits multiple actions then stops on release")
	repeat.free()
	var gesture := node("GestureArea",Vector2.ZERO,Vector2(300,200))
	gesture.tap.connect(func(_p): count("tap"))
	gesture.double_tap.connect(func(_p): count("double"))
	gesture.pan.connect(func(_p): count("pan"))
	gesture.swipe.connect(func(_p): count("swipe"))
	gesture.pinch.connect(func(_p): count("pinch"))
	gesture.rotate.connect(func(_p): count("rotate"))
	await tick()
	mouse(true,Vector2(20,20)); mouse(false,Vector2(20,20))
	mouse(true,Vector2(20,20)); mouse(false,Vector2(20,20))
	check(counts.get("tap",0)==1 and counts.get("double",0)==1,"mouse tap and double tap")
	mouse(true,Vector2(20,20))
	var motion := InputEventMouseMotion.new()
	motion.position = Vector2(120,20)
	motion.relative = Vector2(100,0)
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	root.push_input(motion)
	mouse(false,Vector2(120,20))
	check(counts.get("pan",0)>0 and counts.get("swipe",0)>0,"pan and swipe recognition")
	touch(0,true,Vector2(30,30)); touch(1,true,Vector2(100,30))
	var drag := InputEventScreenDrag.new()
	drag.index = 1; drag.position = Vector2(140,60); drag.relative = Vector2(40,30)
	root.push_input(drag)
	touch(0,false,Vector2(30,30)); touch(1,false,Vector2(140,60))
	check(counts.get("pinch",0)>0 and counts.get("rotate",0)>0,"multi-touch pinch and rotation")
	gesture.free()
	var source := node("DragArea",Vector2(0,0),Vector2(100,100))
	var target := node("DropArea",Vector2(150,0),Vector2(100,100))
	check(not source.is_processing() and not target.is_processing(),"idle drag/drop areas skip frame processing")
	source.drag_started.connect(func(_payload): count("drag_started"))
	source.drag_finished.connect(func(success):
		if success: count("drag_success"))
	source.data = "card payload"; source.drag_type = "card"
	target.allowed_types = PackedStringArray(["card"])
	target.dropped.connect(func(data):
		if data == "card payload": count("drop"))
	await tick()
	mouse(true,Vector2(20,20))
	motion.position = Vector2(60,20); motion.relative=Vector2(40,0)
	root.push_input(motion)
	check(root.gui_is_dragging() and counts.get("drag_started",0)==1,"custom drag threshold starts native dragging")
	await process_frame
	motion.position = Vector2(170,20); motion.relative=Vector2(110,0)
	root.push_input(motion)
	await process_frame
	mouse(false,Vector2(170,20))
	await tick()
	check(counts.get("drop",0)==1,"native drag/drop routes filtered payload")
	source.free(); target.free()
	if DisplayServer.get_name() != "headless": await render_checks()
	print("[NodesTest] failures=",failures)
	quit(failures)
func mouse(pressed: bool, position: Vector2) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT; event.pressed=pressed; event.position=position
	if not mouse_entered:
		root.notify_mouse_entered()
		mouse_entered = true
	root.push_input(event)
func touch(index: int, pressed: bool, position: Vector2) -> void:
	var event := InputEventScreenTouch.new()
	event.index=index; event.pressed=pressed; event.position=position
	root.push_input(event)
func render_checks() -> void:
	var segment := node("SegmentProgressBar",Vector2(0,0),Vector2(200,30))
	segment.segments=4; segment.value=2.5; segment.gap=4
	var gradient := node("GradientRect",Vector2(0,50),Vector2(200,60))
	gradient.gradient.colors=PackedColorArray([Color.RED,Color.BLUE])
	var shape := node("ShapeRect",Vector2(220,50),Vector2(120,60))
	shape.shadow_color=Color.TRANSPARENT
	shape.fill_color=Color.GREEN
	var arc := node("ArcProgress",Vector2(350,10),Vector2(100,100))
	await create_timer(0.2).timeout
	var pixels := root.get_texture().get_image()
	pixels.save_png("res://nodes-render.png")
	check(pixels.get_pixel(10,10).g>0.5 and pixels.get_pixel(180,10).g<0.4,"segment progress GPU rendering")
	check(pixels.get_pixel(10,80).r>0.8 and pixels.get_pixel(190,80).b>0.8,"gradient shader GPU rendering")
	check(pixels.get_pixel(444,60).b>0.8,"batched arc GPU rendering")
	check(pixels.get_pixel(280,80).g>0.8 and pixels.get_pixel(220,50).g<0.5,"rounded shape shader GPU rendering")
	var image := Image.create(60,60,false,Image.FORMAT_RGBA8)
	image.fill(Color.RED)
	image.fill_rect(Rect2i(10,10,40,40),Color.BLUE)
	var page := ImageTexture.create_from_image(image)
	var sprite := AtlasTexture.new()
	sprite.atlas=page; sprite.region=Rect2(10,10,40,40)
	var atlas: Resource = ClassDB.instantiate("SpriteAtlas")
	atlas.sprites={"panel":sprite}
	segment.sprite_atlas=atlas; segment.fill_sprite=&"panel"
	segment.fill_color=Color.WHITE; segment.value=4
	await create_timer(0.1).timeout
	pixels=root.get_texture().get_image()
	check(pixels.get_pixel(10,10).b>0.9 and pixels.get_pixel(10,10).r<0.1,"SegmentProgressBar selects a sprite from its atlas")
	var patch: Resource = ClassDB.instantiate("MultiPatchSprite")
	patch.texture=sprite; patch.x_cuts=PackedInt32Array([10,30])
	patch.y_cuts=PackedInt32Array([10,30])
	segment.fill_patch=patch; segment.value=0.5
	await create_timer(0.1).timeout
	pixels=root.get_texture().get_image()
	check(pixels.get_pixel(10,10).b>0.9 and pixels.get_pixel(35,10).b<0.5,"MultiPatch segment fill preserves the half-fill clip")
	var replacement := Image.create(8,8,false,Image.FORMAT_RGBA8)
	replacement.fill(Color.GREEN)
	atlas.sprites={"panel":ImageTexture.create_from_image(replacement)}
	segment.fill_patch=null
	await create_timer(0.1).timeout
	pixels=root.get_texture().get_image()
	check(pixels.get_pixel(10,10).g>0.9 and pixels.get_pixel(10,10).b<0.1, "atlas replacement refreshes the selected sprite")
	segment.free(); gradient.free(); shape.free(); arc.free()
	var gallery: PackedScene = load("res://addons/meowody_nodes/examples/gallery.tscn")
	root.add_child(gallery.instantiate())
	await create_timer(0.3).timeout
	for label in root.find_children("*", "AutoSizeLabel", true, false):
		check(label.get_line_count()==label.get_visible_line_count(), "multilingual wrapping fits all visible lines")
	root.get_texture().get_image().save_png("res://nodes-gallery.png")
