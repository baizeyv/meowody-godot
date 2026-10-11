@tool
extends EditorInspectorPlugin

var manager: EditorPlugin

func _can_handle(object: Object) -> bool:
	return object is Node or object is Resource

func _parse_begin(object: Object) -> void:
	var action: Callable
	if object.get_class() == "MultiPatchSprite":
		action = manager.open_sprite.bind(object)
	elif object.get_class() == "SpriteAtlas":
		action = manager.open_atlas.bind(object)
	elif object is Texture2D:
		action = manager.open_texture.bind(object)
	if action.is_valid():
		var button := Button.new()
		button.text = "Open MultiPatch Editor"
		button.pressed.connect(action)
		add_custom_control(button)

func _parse_property(_object: Object, type: Variant.Type, name: String, hint: PropertyHint,
		hint_string: String, _usage: int, _wide: bool) -> bool:
	if type == TYPE_OBJECT and hint == PROPERTY_HINT_RESOURCE_TYPE and hint_string == "MultiPatchSprite":
		var property := preload("sprite_property.gd").new()
		property.manager = manager
		add_property_editor(name, property)
		return true
	return false
