extends SceneTree
func _initialize():
	var im = Image.create(13, 17, false, Image.FORMAT_RGBA8)
	im.fill(Color.RED)
	var tex = ImageTexture.create_from_image(im)
	print('image saved: ', ResourceSaver.save(tex, 'res://atlas-probe.res', ResourceSaver.FLAG_COMPRESS))
	for mode in [Image.COMPRESS_ETC2, Image.COMPRESS_ASTC, Image.COMPRESS_BPTC]:
		var cp = im.duplicate()
		var err = cp.compress_from_channels(mode, Image.USED_CHANNELS_RGBA)
		print('compress ', mode, ' ', err, ' size ', cp.get_size(), ' format ', cp.get_format())
		var texture = ImageTexture.create_from_image(cp)
		print('data: ', texture.get_image().get_format() if texture.get_image() else -1)
	quit()
