@tool
extends EditorProperty
var choices := OptionButton.new()
var names := PackedStringArray()
var observed: Resource
func _init() -> void:
	add_child(choices)
	add_focusable(choices)
	choices.item_selected.connect(func(index: int):
		emit_changed(get_edited_property(), StringName(names[index])))
func _update_property() -> void:
	var object := get_edited_object()
	var atlas: Resource = object.get("sprite_atlas")
	if atlas != observed:
		if observed != null: observed.changed.disconnect(_atlas_changed)
		observed = atlas
		if observed != null: observed.changed.connect(_atlas_changed)
	var selected: String = str(object.get(get_edited_property()))
	names = PackedStringArray([""])
	if atlas != null:
		names.append_array(atlas.get_sprite_names())
	if not selected in names:
		names.append(selected)
	choices.clear()
	for sprite in names:
		choices.add_item("None / Use Texture" if sprite.is_empty() else sprite)
	choices.select(names.find(selected))
func _set_read_only(value: bool) -> void:
	choices.disabled = value

func _process(_delta: float) -> void:
	var object := get_edited_object()
	if is_instance_valid(object) and object.get("sprite_atlas") != observed:
		_update_property()
func _atlas_changed() -> void:
	_update_property.call_deferred()
func _exit_tree() -> void:
	if observed != null:
		observed.changed.disconnect(_atlas_changed)
		observed = null
