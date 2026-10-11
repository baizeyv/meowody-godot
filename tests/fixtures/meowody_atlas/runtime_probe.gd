extends SceneTree

func _initialize() -> void:
	var sprite: Texture2D = load("res://art/icons/red.png")
	if not sprite is AtlasTexture or sprite.get_size() != Vector2(20, 10):
		push_error("Exported PNG did not resolve to its atlas.")
		quit(1)
		return
	var expected := Image.FORMAT_BPTC_RGBA
	if OS.get_cmdline_user_args().has("--android"):
		expected = Image.FORMAT_ETC2_RGBA8
	if sprite.atlas.get_image().get_format() != expected:
		push_error("Export did not use the requested platform compression: " + str(sprite.atlas.get_image().get_format()))
		quit(1)
		return
	var catalog: Resource = load("res://runtime_atlas.tres")
	if catalog == null or catalog.get_class() != "SpriteAtlas" or catalog.get_sprite("red") == null or catalog.get_script() != null:
		push_error("Native C++ SpriteAtlas lookup failed in exported runtime.")
		quit(1)
		return
	print("[AtlasTest] PASS native C++ SpriteAtlas lookup in exported runtime")
	print("[AtlasTest] PASS exported original PNG loads with the selected platform GPU format, without the editor plugin")
	quit()
