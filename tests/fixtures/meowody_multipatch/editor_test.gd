@tool
extends RefCounted

const Atlas = preload("res://addons/meowody_atlas/ui_sprite_atlas.gd")
const Builder = preload("res://addons/meowody_atlas/builder.gd")
var failures := 0

func check(condition: bool, message: String) -> void:
	print("[MultiPatchTest] ", "PASS " if condition else "FAIL ", message)
	if not condition:
		failures += 1

func find_plugin(node: Node) -> EditorPlugin:
	if node is EditorPlugin and node.get_script() != null and node.get_script().resource_path == "res://addons/meowody_atlas/plugin.gd":
		return node
	for child in node.get_children():
		var found := find_plugin(child)
		if found != null:
			return found
	return null

func run(plugin: EditorPlugin) -> void:
	await plugin.get_tree().create_timer(1.0).timeout
	var atlas_plugin := find_plugin(plugin.get_tree().root)
	var image := Image.create(40, 40, false, Image.FORMAT_RGBA8)
	image.fill(Color.RED)
	image.fill_rect(Rect2i(10, 0, 20, 40), Color.GREEN)
	image.fill_rect(Rect2i(30, 0, 10, 40), Color.BLUE)
	image.save_png("res://panel.png")
	image.fill(Color.TRANSPARENT)
	image.fill_rect(Rect2i(4, 6, 30, 28), Color.CYAN)
	image.save_png("res://trimmed.png")
	await atlas_plugin._refresh_imports(PackedStringArray())
	var sprite: Resource = ClassDB.instantiate("MultiPatchSprite")
	sprite.texture = load("res://panel.png")
	sprite.x_cuts = PackedInt32Array([10, 20, 30])
	sprite.y_cuts = PackedInt32Array([10, 20, 30])
	var patches: Array = sprite.get_draw_regions(Vector2(100, 60))
	check(patches.size() == 16 and patches[0].destination.size == Vector2(10, 10) and patches[5].destination.size == Vector2(40, 20), "4x4 preserves corners and distributes stretch space")
	sprite.x_weights = PackedFloat32Array([1, 1, 3, 1])
	patches = sprite.get_draw_regions(Vector2(100, 60))
	check(patches[1].destination.size.x == 20 and patches[2].destination.size.x == 60, "flexible weights distribute remaining width")
	patches = sprite.get_draw_regions(Vector2(10, 10))
	check(patches.size() == 4 and patches[0].destination.size == Vector2(5, 5), "undersized targets shrink fixed borders safely")
	sprite.x_cuts = PackedInt32Array([20, 10])
	check(not sprite.get_validation_error().is_empty() and sprite.get_draw_regions(Vector2(100, 60)).is_empty(), "invalid cuts produce no unsafe draw calls")
	sprite.x_cuts = PackedInt32Array()
	sprite.y_cuts = PackedInt32Array()
	sprite.x_weights = PackedFloat32Array()
	sprite.x_modes = PackedInt32Array([2])
	patches = sprite.get_draw_regions(Vector2(95, 40))
	check(patches.size() == 3 and patches[2].source.size.x == 15 and patches[2].destination.size.x == 15, "Tile crops its last repetition")
	sprite.x_modes = PackedInt32Array([3])
	patches = sprite.get_draw_regions(Vector2(95, 40))
	check(patches.size() == 2 and patches[1].source.size.x == 40 and patches[1].destination.size.x == 47.5, "Tile Fit keeps complete repetitions")
	sprite.x_modes = PackedInt32Array([9])
	check(not sprite.get_validation_error().is_empty(), "invalid modes rejected")
	sprite.x_modes = PackedInt32Array()
	plugin.open_texture(sprite.texture)
	var panel: Control = plugin.panel
	panel.preset(4)
	check(panel.dirty and panel.draft.x_cuts.size() == 3, "editor preset updates draft")
	panel.undo.undo()
	check(not panel.dirty and panel.draft.x_cuts.is_empty(), "undo restores original draft")
	panel.undo.redo()
	panel._move_cut(8)
	check(panel.draft.x_cuts[0] == 8, "numeric cut editing")
	panel.undo.undo()
	# Exercise the actual mouse input path, not just the data setters.
	var canvas: Control = panel.canvas
	canvas.fit()
	var press := InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = canvas.origin() + Vector2(10, 5) * canvas.zoom
	canvas._gui_input(press)
	var motion := InputEventMouseMotion.new()
	motion.position = canvas.origin() + Vector2(12, 5) * canvas.zoom
	canvas._gui_input(motion)
	press.pressed = false
	press.position = motion.position
	canvas._gui_input(press)
	check(panel.draft.x_cuts[0] == 12, "mouse drag snaps a cut to source pixels")
	panel.undo.undo()
	check(panel.draft.x_cuts[0] == 10, "mouse drag is one undo action")
	panel._set_mode(2)
	panel._add_cut()
	check(panel.draft.x_modes[0] == 2 and panel.draft.x_modes[1] == 2 and panel.draft.x_modes[-1] == 0, "adding a cut preserves existing segment modes")
	panel._remove_cut()
	check(panel.draft.x_cuts == PackedInt32Array([10,20,30]) and panel.draft.x_modes[0] == 2, "removing a cut merges only adjacent segments")
	panel.undo.undo()
	panel.undo.undo()
	panel.undo.undo()
	panel.apply()
	check(not panel.dirty and panel.saved.get_script() == null and FileAccess.file_exists("res://panel.png.mpsprite.tres"), "Apply saves native sidecar")
	sprite = panel.saved
	var cached_only := sprite.duplicate()
	ResourceSaver.save(cached_only, "res://cached_only.mpsprite.tres")
	cached_only = load("res://cached_only.mpsprite.tres")
	var wrapper: Texture2D = ClassDB.instantiate("MultiPatchTexture")
	wrapper.sprite = sprite
	ResourceSaver.save(wrapper, "res://patch_texture.tres")
	var root := Control.new()
	root.name = "Sample"
	var direct: Control = ClassDB.instantiate("MultiPatchRect")
	direct.name = "Direct"
	direct.sprite = sprite
	direct.size = Vector2(100, 60)
	root.add_child(direct)
	direct.owner = root
	var textured := TextureRect.new()
	textured.name = "Textured"
	textured.texture = wrapper
	textured.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	textured.size = Vector2(100, 60)
	textured.position.y = 70
	root.add_child(textured)
	textured.owner = root
	var scene := PackedScene.new()
	scene.pack(root)
	ResourceSaver.save(scene, "res://sample.tscn")
	root.free()
	EditorInterface.open_scene_from_path("res://sample.tscn")
	await plugin.get_tree().process_frame
	EditorInterface.edit_node(EditorInterface.get_edited_scene_root().get_node("Direct"))
	await plugin.get_tree().process_frame
	var custom_property := false
	for property in EditorInterface.get_inspector().find_children("*", "EditorProperty", true, false):
		if property.get_script() != null and property.get_script().resource_path == "res://addons/meowody_multipatch/sprite_property.gd":
			custom_property = true
	check(custom_property, "native Sprite property uses the image-drop inspector editor")
	var atlas := Atlas.new()
	atlas.packables = PackedStringArray(["res://panel.png", "res://trimmed.png"])
	atlas.trim_transparent = true
	atlas.auto_repack = false
	atlas.max_size = 128
	ResourceSaver.save(atlas, "res://TestAtlas.tres")
	atlas = load("res://TestAtlas.tres")
	await atlas_plugin.request(atlas, false)
	await plugin.get_tree().process_frame
	var edited: Control = EditorInterface.get_edited_scene_root()
	check(cached_only.texture is AtlasTexture, "cached sidecar outside scenes follows atlas transition")
	DirAccess.remove_absolute("res://cached_only.mpsprite.tres")
	check(edited.get_node("Direct").sprite.texture is AtlasTexture, "open native MultiPatchRect follows PNG to AtlasTexture transition")
	check(edited.get_node("Textured").texture.sprite.texture is AtlasTexture, "nested MultiPatchTexture follows atlas transition")
	check(panel.draft.texture is AtlasTexture and panel.draft.x_cuts == PackedInt32Array([10,20,30]), "active editor draft follows atlas without losing cuts")
	check(FileAccess.get_file_as_string("res://panel.png.mpsprite.tres").contains("res://panel.png"), "sidecar retains source path instead of packed page coordinates")
	var manifest := Builder.read_manifest(Builder.manifest_path(atlas))
	check(plugin.original_texture(load(manifest.sprites["res://panel.png"].resource)).resource_path == "res://panel.png", "generated atlas sprite resolves original source for editing")
	var old_region: Rect2 = edited.get_node("Direct").sprite.texture.region
	var old_page: String = edited.get_node("Direct").sprite.texture.atlas.resource_path
	var large := Image.create(80, 80, false, Image.FORMAT_RGBA8)
	large.fill(Color.MAGENTA)
	large.save_png("res://large.png")
	await atlas_plugin._refresh_imports(PackedStringArray())
	atlas.packables.append("res://large.png")
	ResourceSaver.save(atlas, atlas.resource_path)
	await atlas_plugin.request(atlas, false)
	check((edited.get_node("Direct").sprite.texture.region != old_region or edited.get_node("Direct").sprite.texture.atlas.resource_path != old_page), "repack moves the source region in the atlas")
	check(edited.get_node("Direct").sprite.get_draw_regions(Vector2(100,60)).size() == 16, "repacking retains valid logical patch layout")
	plugin.open_texture(load("res://panel.png"))
	panel._change_region(Rect2i(0, 0, 20, 40))
	panel.undo.undo()
	check(panel.draft.get_effective_region() == Rect2i(0,0,40,40), "region edits undo with cut data")
	panel.request_open(load("res://panel.png"), null)
	panel.title.text = "Second"
	panel._change_name()
	panel._change_region(Rect2i(0,0,20,40))
	panel.apply()
	await atlas_plugin._refresh_imports(PackedStringArray())
	check(plugin.find_sprites(load("res://panel.png")).size() == 2, "multiple sidecars coexist on the same source")
	plugin.pick_sprite(load("res://panel.png"), func(_value): pass)
	check(plugin.choose.visible and plugin.choices.item_count == 2, "source image drop offers sprite selection")
	plugin.choose.hide()
	plugin.open_sprite(sprite)
	await plugin.get_tree().process_frame
	if DisplayServer.get_name() != "headless":
		await render_check(plugin, scene)
		await plugin.get_tree().create_timer(0.5).timeout
		EditorInterface.get_base_control().get_viewport().get_texture().get_image().save_png("res://editor-preview.png")
	await plugin.get_tree().process_frame
	var platform := EditorExportPlatformWindows.new()
	var preset := platform.create_preset()
	check(platform.export_pack(preset, false, ProjectSettings.globalize_path("res://multipatch.pck")) == OK, "PCK export succeeds with both plugins enabled")
	print("[MultiPatchTest] failures=", failures)
	plugin.get_tree().quit(0 if failures == 0 else 1)

func render_check(plugin: EditorPlugin, scene: PackedScene) -> void:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(110, 390)
	viewport.transparent_bg = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	plugin.add_child(viewport)
	var sample := scene.instantiate()
	viewport.add_child(sample)
	var flipped := sample.get_node("Textured").duplicate()
	flipped.position.y = 140
	flipped.flip_h = true
	sample.add_child(flipped)
	var trimmed: Resource = ClassDB.instantiate("MultiPatchSprite")
	trimmed.texture = load("res://trimmed.png")
	trimmed.x_cuts = PackedInt32Array([10, 30])
	trimmed.y_cuts = PackedInt32Array([10, 30])
	var trimmed_rect: Control = ClassDB.instantiate("MultiPatchRect")
	trimmed_rect.sprite = trimmed
	trimmed_rect.size = Vector2(100, 60)
	trimmed_rect.position.y = 210
	sample.add_child(trimmed_rect)
	var cropped := sample.get_node("Textured").duplicate()
	cropped.position.y = 280
	cropped.size = Vector2(60, 100)
	cropped.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
	sample.add_child(cropped)
	await plugin.get_tree().create_timer(0.3).timeout
	var pixels := viewport.get_texture().get_image()
	pixels.save_png("res://native-preview.png")
	for y in [5, 75]:
		check(pixels.get_pixel(5,y).r > 0.9 and pixels.get_pixel(20,y).g > 0.9 and pixels.get_pixel(95,y).b > 0.9, "native rendering preserves border colors at row %d" % y)
	check(pixels.get_pixel(5,145).b > 0.9 and pixels.get_pixel(95,145).r > 0.9, "TextureRect horizontal flip")
	check(pixels.get_pixel(1,211).a < 0.01 and pixels.get_pixel(5,217).g > 0.9 and pixels.get_pixel(99,240).a < 0.01, "trimmed atlas preserves transparent logical margins")
	check(pixels.get_pixel(5,285).g > 0.9 and pixels.get_pixel(55,285).g > 0.9 and pixels.get_pixel(65,285).a < 0.01, "TextureRect aspect-covered cropping clips the patch drawing")
	viewport.queue_free()
