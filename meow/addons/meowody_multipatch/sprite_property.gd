@tool
extends EditorProperty

var manager: EditorPlugin
var picker: EditorResourcePicker
var target: Button

class DropTarget extends Button:
	signal texture_dropped(texture: Texture2D)
	func texture_from(data: Variant) -> Texture2D:
		if not data is Dictionary:
			return null
		var resource: Resource = data.get("resource")
		if resource == null and data.get("files", []).size() == 1 and ResourceLoader.exists(data.files[0]):
			resource = load(data.files[0])
		return resource if resource is Texture2D and resource.get_class() != "MultiPatchTexture" else null
	func _can_drop_data(_position: Vector2, data: Variant) -> bool:
		return not disabled and texture_from(data) != null
	func _drop_data(_position: Vector2, data: Variant) -> void:
		texture_dropped.emit(texture_from(data))

func _init() -> void:
	var column := VBoxContainer.new()
	add_child(column)
	picker = EditorResourcePicker.new()
	picker.base_type = "MultiPatchSprite"
	column.add_child(picker)
	picker.resource_changed.connect(_assign)
	picker.resource_selected.connect(func(resource: Resource, _inspect: bool): EditorInterface.edit_resource(resource))
	target = DropTarget.new()
	target.text = "Drop source image here / Edit Sprite"
	target.texture_dropped.connect(func(texture: Texture2D): manager.pick_sprite(texture, _assign))
	target.pressed.connect(func():
		if picker.edited_resource != null:
			manager.open_sprite(picker.edited_resource))
	column.add_child(target)
	set_bottom_editor(column)

func _assign(value: Resource) -> void:
	if not is_read_only():
		emit_changed(get_edited_property(), value)
		picker.edited_resource = value

func _update_property() -> void:
	picker.edited_resource = get_edited_object().get(get_edited_property())

func _set_read_only(value: bool) -> void:
	picker.editable = not value
	target.disabled = value
