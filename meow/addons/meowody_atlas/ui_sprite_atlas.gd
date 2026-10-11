@tool
class_name UISpriteAtlas
extends Resource

const Profile = preload("atlas_platform_settings.gd")
enum SizeMode { TIGHT, POWER_OF_TWO, SQUARE_POWER_OF_TWO }
enum PackingQuality { FAST, BALANCED, BEST }

@export_group("Packables")
@export var packables := PackedStringArray()
@export var include_subfolders := true
@export var auto_repack := true

@export_group("Packing")
@export_enum("256:256", "512:512", "1024:1024", "2048:2048", "4096:4096", "8192:8192") var max_size := 2048
@export var size_mode: SizeMode = SizeMode.TIGHT
@export var packing_quality: PackingQuality = PackingQuality.BEST
@export_range(0, 32) var padding := 2
@export_range(0, 16) var extrude := 1
@export var trim_transparent := false

@export_group("Platform Compression")
@export var default_settings: UIAtlasPlatformSettings = Profile.new()
@export var android: UIAtlasPlatformSettings = Profile.new()
@export var ios: UIAtlasPlatformSettings = Profile.new()
@export var desktop: UIAtlasPlatformSettings = Profile.new()
@export var web: UIAtlasPlatformSettings = Profile.new()

# Stable across moving or renaming this asset. Generated files belong only to this ID.
@export_storage var atlas_id := ""


func settings() -> Dictionary:
	var profiles := {"default": default_settings.snapshot() if default_settings != null else Profile.new().snapshot()}
	for target in ["android", "ios", "desktop", "web"]:
		var profile: UIAtlasPlatformSettings = get(target)
		if profile != null and profile.override_enabled:
			profiles[target] = profile.snapshot()
	return {"packables": Array(packables), "recursive": include_subfolders, "max_size": max_size,
		"size_mode": size_mode, "quality": packing_quality, "padding": padding,
		"extrude": extrude, "trim": trim_transparent, "profiles": profiles}
