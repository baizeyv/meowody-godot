@tool
extends RefCounted

const EXTENSIONS := ["png", "jpg", "jpeg", "webp", "bmp", "tga", "svg", "hdr", "exr"]

static func supports(path: String) -> bool:
	return path.get_extension().to_lower() in EXTENSIONS
