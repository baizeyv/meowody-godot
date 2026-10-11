@tool
extends RefCounted

const Sources = preload("source_images.gd")

# Image work only: safe to run off the editor thread, with no native extension instances.
static func collect(settings: Dictionary) -> PackedStringArray:
	var found := {}
	for entry in settings.packables:
		var path := str(entry).simplify_path()
		if path != "res://":
			path = path.trim_suffix("/")
		if not path.begins_with("res://") or path.begins_with("res://generated/ui_atlases"):
			continue
		if DirAccess.dir_exists_absolute(path):
			_collect_folder(path, settings.recursive, found)
		elif Sources.supports(path) and FileAccess.file_exists(path):
			found[path] = true
	var result := PackedStringArray(found.keys())
	result.sort()
	return result


static func _collect_folder(path: String, recursive: bool, found: Dictionary) -> void:
	var directory := DirAccess.open(path)
	if directory == null:
		return
	for file in directory.get_files():
		if Sources.supports(file):
			found[path.path_join(file)] = true
	if recursive:
		for folder in directory.get_directories():
			if not folder.begins_with(".") and not path.path_join(folder).begins_with("res://generated/ui_atlases"):
				_collect_folder(path.path_join(folder), true, found)


static func fingerprint(settings: Dictionary, paths: PackedStringArray) -> String:
	var parts := [JSON.stringify(settings)]
	for path in paths:
		parts.append(path + ":" + FileAccess.get_md5(path))
	return "\n".join(parts).sha256_text()


static func build(settings: Dictionary) -> Dictionary:
	var limit: int = settings.max_size
	if limit < 16 or limit > 8192 or settings.padding < 0 or settings.extrude < 0:
		return {"error": "Invalid atlas size or padding."}
	var paths := collect(settings)
	var input_fingerprint := fingerprint(settings, paths)
	var sprites: Array[Dictionary] = []
	var border: int = settings.padding + settings.extrude
	var min_width := 1
	for path in paths:
		var image := Image.new()
		if image.load(ProjectSettings.globalize_path(path)) != OK or image.is_empty():
			return {"error": "Cannot read image: " + path}
		if image.is_compressed() and image.decompress() != OK:
			return {"error": "Cannot decompress image: " + path}
		image.convert(Image.FORMAT_RGBA8)
		var original := image.get_size()
		var used := image.get_used_rect() if settings.trim else Rect2i(Vector2i.ZERO, original)
		# Preserve the size of fully transparent sprites too.
		if used.size == Vector2i.ZERO:
			used = Rect2i(0, 0, 1, 1)
		image = image.get_region(used)
		var size := used.size + Vector2i.ONE * border * 2
		if size.x > limit or size.y > limit:
			return {"error": "%s (%s plus padding) exceeds Max Size %d." % [path, original, limit]}
		min_width = maxi(min_width, size.x)
		sprites.append({"path": path, "image": image, "size": size, "original": original, "trim": used})
	var best := {"pages": [], "area": 0, "longest": 0}
	if not sprites.is_empty():
		best = {}
		var trials: int = [1, 8, 20][clampi(settings.quality, 0, 2)]
		var orders: int = [1, 2, 4][clampi(settings.quality, 0, 2)]
		var widths := {limit: true, min_width: true}
		for trial in range(trials):
			widths[mini(limit, min_width + int(float(limit - min_width) * trial / trials))] = true
		for order in range(orders):
			var sorted := sprites.duplicate()
			sorted.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
				var sa := _sort_score(a.size, order)
				var sb := _sort_score(b.size, order)
				return a.path < b.path if sa == sb else sa > sb)
			for heuristic in range(1 if settings.quality == 0 else 2):
				for width in widths:
					var layout := _layout(sorted, width, limit, settings.size_mode, heuristic)
					if best.is_empty() or _better(layout, best):
						best = layout
	var pages: Array[Image] = []
	var entries := {}
	var used_area := 0
	for page in best.pages:
		var canvas := Image.create(page.size.x, page.size.y, false, Image.FORMAT_RGBA8)
		canvas.fill(Color.TRANSPARENT)
		for item in page.items:
			var sprite: Dictionary = item.sprite
			var position: Vector2i = item.rect.position + Vector2i.ONE * border
			_blit_extruded(canvas, sprite.image, position, settings.extrude)
			entries[sprite.path] = {"page": pages.size(),
				"region": [position.x, position.y, sprite.trim.size.x, sprite.trim.size.y],
				"margin": [sprite.trim.position.x, sprite.trim.position.y,
					sprite.original.x - sprite.trim.size.x, sprite.original.y - sprite.trim.size.y]}
			used_area += sprite.trim.size.x * sprite.trim.size.y
		pages.append(canvas)
	return {"error": "", "pages": pages, "sprites": entries, "used_area": used_area,
		"area": best.area, "fingerprint": input_fingerprint, "settings": settings}


static func _sort_score(size: Vector2i, order: int) -> int:
	match order:
		1: return maxi(size.x, size.y)
		2: return size.x
		3: return size.y
	return size.x * size.y


static func _better(a: Dictionary, b: Dictionary) -> bool:
	if a.pages.size() != b.pages.size():
		return a.pages.size() < b.pages.size()
	if a.area != b.area:
		return a.area < b.area
	return a.longest < b.longest


static func _layout(sprites: Array, width: int, height: int, mode: int, heuristic: int) -> Dictionary:
	var remaining := sprites.duplicate()
	var pages := []
	var area := 0
	var longest := 0
	while not remaining.is_empty():
		var free: Array[Rect2i] = [Rect2i(0, 0, width, height)]
		var items := []
		var next := []
		var extent := Vector2i.ONE
		for sprite in remaining:
			var chosen := Rect2i()
			var best_score := Vector2i(2147483647, 2147483647)
			for rect in free:
				var rest: Vector2i = rect.size - sprite.size
				if rest.x < 0 or rest.y < 0:
					continue
				var score := Vector2i(mini(rest.x, rest.y), maxi(rest.x, rest.y))
				if heuristic == 1:
					score = Vector2i(rect.get_area() - sprite.size.x * sprite.size.y, mini(rest.x, rest.y))
				if score.x < best_score.x or (score.x == best_score.x and score.y < best_score.y):
					best_score = score
					chosen = Rect2i(rect.position, sprite.size)
			if chosen.size == Vector2i.ZERO:
				next.append(sprite)
				continue
			items.append({"sprite": sprite, "rect": chosen})
			extent = extent.max(chosen.end)
			free = _split(free, chosen)
		if mode != 0:
			extent = Vector2i(nearest_po2(extent.x), nearest_po2(extent.y))
		if mode == 2:
			extent = Vector2i.ONE * maxi(extent.x, extent.y)
		pages.append({"size": extent, "items": items})
		area += extent.x * extent.y
		longest = maxi(longest, maxi(extent.x, extent.y))
		remaining = next
	return {"pages": pages, "area": area, "longest": longest}


static func _split(free: Array[Rect2i], used: Rect2i) -> Array[Rect2i]:
	var pieces: Array[Rect2i] = []
	for rect in free:
		if not rect.intersects(used):
			pieces.append(rect)
			continue
		if used.position.x > rect.position.x:
			pieces.append(Rect2i(rect.position, Vector2i(used.position.x - rect.position.x, rect.size.y)))
		if used.end.x < rect.end.x:
			pieces.append(Rect2i(used.end.x, rect.position.y, rect.end.x - used.end.x, rect.size.y))
		if used.position.y > rect.position.y:
			pieces.append(Rect2i(rect.position, Vector2i(rect.size.x, used.position.y - rect.position.y)))
		if used.end.y < rect.end.y:
			pieces.append(Rect2i(rect.position.x, used.end.y, rect.size.x, rect.end.y - used.end.y))
	var result: Array[Rect2i] = []
	for i in range(pieces.size()):
		var contained := false
		for j in range(pieces.size()):
			if i != j and pieces[j].encloses(pieces[i]) and (pieces[i] != pieces[j] or j < i):
				contained = true
				break
		if not contained:
			result.append(pieces[i])
	return result


static func _blit_extruded(canvas: Image, source: Image, position: Vector2i, amount: int) -> void:
	var size := source.get_size()
	canvas.blit_rect(source, Rect2i(Vector2i.ZERO, size), position)
	for offset in range(1, amount + 1):
		canvas.blit_rect(source, Rect2i(0, 0, size.x, 1), position - Vector2i(0, offset))
		canvas.blit_rect(source, Rect2i(0, size.y - 1, size.x, 1), position + Vector2i(0, size.y - 1 + offset))
		canvas.blit_rect(source, Rect2i(0, 0, 1, size.y), position - Vector2i(offset, 0))
		canvas.blit_rect(source, Rect2i(size.x - 1, 0, 1, size.y), position + Vector2i(size.x - 1 + offset, 0))
	for x in range(amount):
		for y in range(amount):
			canvas.set_pixelv(position - Vector2i(x + 1, y + 1), source.get_pixel(0, 0))
			canvas.set_pixelv(position + Vector2i(size.x + x, -y - 1), source.get_pixel(size.x - 1, 0))
			canvas.set_pixelv(position + Vector2i(-x - 1, size.y + y), source.get_pixel(0, size.y - 1))
			canvas.set_pixelv(position + size + Vector2i(x, y), source.get_pixel(size.x - 1, size.y - 1))
