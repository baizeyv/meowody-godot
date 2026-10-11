@tool
extends VBoxContainer

var atlas: Resource
var list: ItemList
var search: LineEdit

func _ready() -> void:
	var hint := Label.new()
	hint.text = "Native C++ SpriteAtlas · get_sprite(name)\nUse a full name below, or a unique filename without extension."
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	add_child(hint)
	search = LineEdit.new()
	search.placeholder_text = "Search sprite names..."
	add_child(search)
	list = ItemList.new()
	list.custom_minimum_size.y = 240 * EditorInterface.get_editor_scale()
	list.fixed_icon_size = Vector2i(40, 40)
	add_child(list)
	var copy := Button.new()
	copy.text = "Copy Selected Sprite Name"
	copy.pressed.connect(func():
		if not list.get_selected_items().is_empty():
			DisplayServer.clipboard_set(list.get_item_text(list.get_selected_items()[0])))
	add_child(copy)
	search.text_changed.connect(func(_value: String): _refresh())
	atlas.changed.connect(_refresh)
	_refresh()

func _refresh() -> void:
	# Resource changes can arrive while an ItemList selection event is running.
	_refresh_deferred.call_deferred()

func _refresh_deferred() -> void:
	list.clear()
	for name in atlas.get_sprite_names():
		if search.text.is_empty() or str(name).to_lower().contains(search.text.to_lower()):
			list.add_item(name, atlas.get_sprite(name))
