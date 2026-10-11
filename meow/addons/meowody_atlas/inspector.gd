@tool
extends EditorInspectorPlugin

const Atlas = preload("ui_sprite_atlas.gd")
const AtlasPanel = preload("inspector_panel.gd")
var manager: EditorPlugin

func _can_handle(object: Object) -> bool:
	return object is Atlas or object.get_class() == "SpriteAtlas"

func _parse_begin(object: Object) -> void:
	if object.get_class() == "SpriteAtlas":
		var catalog := preload("catalog_panel.gd").new()
		catalog.atlas = object
		add_custom_control(catalog)
		return
	if not object is Atlas:
		return
	var panel := AtlasPanel.new()
	panel.atlas = object
	panel.manager = manager
	add_custom_control(panel)

func _parse_property(object: Object, _type: Variant.Type, name: String, _hint_type: PropertyHint,
		_hint_string: String, _usage_flags: int, _wide: bool) -> bool:
	return object is Atlas and name in ["packables", "default_settings", "android", "ios", "desktop", "web"]
