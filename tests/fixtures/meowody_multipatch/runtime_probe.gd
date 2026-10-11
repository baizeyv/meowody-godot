extends SceneTree
func _initialize() -> void:
	var sprite: Resource = load("res://panel.png.mpsprite.tres")
	var texture: Texture2D = load("res://patch_texture.tres")
	var scene: PackedScene = load("res://sample.tscn")
	var node := scene.instantiate()
	root.add_child(node)
	if sprite == null or sprite.get_script() != null or not sprite.texture is AtlasTexture or sprite.get_draw_regions(Vector2(100,60)).size() != 16 or texture.get_size() != Vector2(40,40) or node.get_node("Direct").get_class() != "MultiPatchRect":
		print("[MultiPatchTest] FAIL exported native resources")
		quit(1)
	else:
		print("[MultiPatchTest] PASS exported native runtime without EditorPlugin")
		quit()
