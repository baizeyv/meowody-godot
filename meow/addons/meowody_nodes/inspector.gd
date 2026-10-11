@tool
extends EditorInspectorPlugin
var plugin: EditorPlugin
const HELP := {
	"VirtualList": "Only visible cells are instantiated. Use Bind Item or set_binder in C++. Cells are reused: overwrite their complete display state when binding. Editor Preview Count is independent of runtime Item Count.",
	"SnapScrollContainer": "Virtualized item/page/center snapping. Assign an Item Scene, Item Count and Direction. Snap Curve overrides the default easing. Overscroll adds space at the ends.",
	"LoopScrollContainer": "Repeats data indices without an end. Uses one lane, including when the viewport shows multiple copies of a small data set. Item scenes are recycled.",
	"SegmentProgressBar": "Value is measured in segments (3.5 means three and a half). All four directions support partial filling. Assign Sprite Atlas and select sprite names, or assign MultiPatch resources. Priority: Patch, named atlas sprite, Texture. Set tint colors to white to retain the original image colors.",
	"ArcProgress": "Value is 0–1; angles are degrees. Gradient is sampled across the full arc. Thickness and round caps are editable directly in the viewport preview.",
	"NumberLabel": "Value drives the text. Prefix, Suffix, Decimals, Grouping and Compact format it. Runtime changes animate; Rolling slides between the old and new formatted values.",
	"AutoSizeLabel": "Finds the largest font size that fits. Set a real Control size, then configure Min/Max Font Size. Native Label text, wrapping and theme font are used.",
	"GradientRect": "Linear, radial or conic gradient. Center and Radius are normalized to the rectangle. Edit the native Gradient resource to add color stops.",
	"ShapeRect": "Rounded rectangle, capsule, circle or chamfer. Corner Radii order: top-left, top-right, bottom-right, bottom-left. Border, gradient and shadow are independent.",
	"LongPressButton": "Long Pressed and Long Press Progress are additional signals. Native Pressed still works. Movement, release, disabling or losing focus cancel a hold.",
	"RepeatButton": "Connect Repeated for repeating actions. Trigger On Press emits immediately; then Initial Delay, Repeat Interval and Acceleration take effect. Native Pressed is separate.",
	"DragArea": "Covers a draggable item. Data and Drag Type travel together. Assign Ghost Texture or Ghost Scene. Failed drops can animate the ghost back; the original stays in place.",
	"DropArea": "Allowed Types filters DragArea payloads; an empty array accepts all types. Validator can apply additional rules. Dropped receives Data, not the metadata envelope.",
	"GestureArea": "Tap, Double Tap, Long Press, Swipe, Pan, Pinch and Rotate. Pan is in local pixels, Pinch is an incremental factor, Rotate is incremental radians. Overlay children should Ignore mouse input."
}
func _can_handle(object: Object) -> bool:
	return object.get_class() in HELP
func _parse_begin(object: Object) -> void:
	var column := VBoxContainer.new()
	var label := Label.new()
	label.text = HELP[object.get_class()]
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(label)
	if object.is_class("VirtualList"):
		var preview := Button.new()
		preview.text = "Toggle Item Scene Preview"
		preview.tooltip_text = "Instantiates the item scene in the editor. Tool scripts in that scene may run. Disabled previews use safe numbered placeholders."
		preview.pressed.connect(func():
			if not is_instance_valid(object): return
			var undo := plugin.get_undo_redo()
			undo.create_action("Toggle list scene preview")
			undo.add_do_property(object, "preview_in_editor", not object.preview_in_editor)
			undo.add_undo_property(object, "preview_in_editor", object.preview_in_editor)
			undo.commit_action())
		column.add_child(preview)
	elif object.get_class() == "AutoSizeLabel":
		var fit := Button.new()
		fit.text = "Fit Text Now"
		fit.pressed.connect(func():
			if is_instance_valid(object): object.fit_now())
		column.add_child(fit)
	add_custom_control(column)

func _parse_property(object: Object, _type: Variant.Type, name: String, _hint: PropertyHint,
		_hint_string: String, _usage: int, _wide: bool) -> bool:
	if object.get_class() == "SegmentProgressBar" and name in ["background_sprite", "fill_sprite", "separator_sprite"]:
		add_property_editor(name, preload("atlas_sprite_property.gd").new())
		return true
	return false
