@tool
class_name UIAtlasPlatformSettings
extends Resource

enum Compression { LOSSLESS, UNCOMPRESSED, ETC2_RGBA8, ASTC_4X4, ASTC_8X8, BC7 }

@export var override_enabled := false
@export var compression: Compression = Compression.LOSSLESS
@export var mipmaps := false


func snapshot() -> Dictionary:
	return {"compression": compression, "mipmaps": mipmaps}
