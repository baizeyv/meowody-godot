@tool
extends RefCounted
const TYPES := ["VirtualList","SnapScrollContainer","LoopScrollContainer","SegmentProgressBar","NumberLabel","AutoSizeLabel","GradientRect","ArcProgress","ShapeRect","LongPressButton","RepeatButton","DragArea","DropArea","GestureArea"]
var failures := 0
func check(ok: bool, message: String) -> void:
	print("[NodesTest] ", "PASS " if ok else "FAIL ", message)
	if not ok: failures += 1
func run(plugin: EditorPlugin) -> void:
	await plugin.get_tree().create_timer(1).timeout
	var root := Control.new()
	root.name = "NativeNodes"
	plugin.add_child(root)
	for type in TYPES:
		check(ClassDB.class_exists(type), type + " registered")
		var node: Control = ClassDB.instantiate(type)
		node.name = type
		root.add_child(node)
		node.owner = root
		node.size = Vector2(240,120)
	await plugin.get_tree().process_frame
	await plugin.get_tree().process_frame
	check(root.get_node("VirtualList").get_visible_node_count() > 0, "safe editor list preview")
	var segment: Control = root.get_node("SegmentProgressBar")
	var atlas: Resource = ClassDB.instantiate("SpriteAtlas")
	var image := Image.create(16,16,false,Image.FORMAT_RGBA8)
	image.fill(Color.GREEN)
	atlas.sprites={"heart":ImageTexture.create_from_image(image)}
	segment.sprite_atlas=atlas
	segment.fill_sprite=&"heart"
	var patch: Resource = ClassDB.instantiate("MultiPatchSprite")
	patch.texture=atlas.get_sprite("heart")
	patch.x_cuts=PackedInt32Array([4,12])
	segment.fill_patch=patch
	var scene := PackedScene.new()
	check(scene.pack(root) == OK and ResourceSaver.save(scene,"res://native_nodes.tscn") == OK, "all native nodes serialize")
	var content := FileAccess.get_file_as_string("res://native_nodes.tscn")
	check(not content.contains("shader_type") and not content.contains("parent=\"VirtualList\""), "internal shaders and preview cells are not serialized")
	root.queue_free()
	EditorInterface.open_scene_from_path("res://native_nodes.tscn")
	await plugin.get_tree().process_frame
	EditorInterface.edit_node(EditorInterface.get_edited_scene_root().get_node("SegmentProgressBar"))
	await plugin.get_tree().process_frame
	await plugin.get_tree().process_frame
	var editors := 0
	for property in EditorInterface.get_inspector().find_children("*","EditorProperty",true,false):
		if property.get_script() != null and property.get_script().resource_path.ends_with("atlas_sprite_property.gd"):
			editors += 1
	check(editors==3,"SegmentProgressBar provides three atlas sprite selectors")
	var loaded: Control = EditorInterface.get_edited_scene_root().get_node("SegmentProgressBar")
	check(loaded.sprite_atlas.get_sprite("heart") != null and loaded.fill_patch.x_cuts == PackedInt32Array([4,12]),"atlas and MultiPatch selections serialize with the scene")
	var platform := EditorExportPlatformWindows.new()
	var preset := platform.create_preset()
	check(platform.export_pack(preset,false,ProjectSettings.globalize_path("res://nodes.pck")) == OK,"native nodes export to PCK")
	print("[NodesTest] failures=",failures)
	plugin.get_tree().quit(failures)
