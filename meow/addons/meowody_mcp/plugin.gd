@tool
extends EditorPlugin

## Based on YasiruRF/godot-mcp, commit 25ccc4e201fcd2d43f952e19e39460daa43d45b0.
## See LICENSE. Only Godot-owned undo methods hold scene objects; no native callbacks.
const ENDPOINT := "res://.godot/meowody_mcp/bridge.json"
const BINDING_SCRIPT := "res://addons/meowody_binding/plugin.gd"
const BAD_NAME_CHARS := ".:@/\"%\\"
const MUTATIONS := ["open_scene", "add_node", "set_properties", "remove_node", "save_scene", "run_scene", "stop", "select_node", "undo", "redo"]

var _tcp_server := TCPServer.new()
var _peers: Array[WebSocketPeer] = []
var _token := ""
var _project_path := ""
var _opening_scene := false
var _alive := false

func _enter_tree() -> void:
	_project_path = _normalize_path(ProjectSettings.globalize_path("res://"))
	if FileAccess.file_exists(ENDPOINT):
		var previous = JSON.parse_string(FileAccess.get_file_as_string(ENDPOINT))
		if previous is Dictionary and int(previous.get("pid", -1)) > 0 and OS.is_process_running(int(previous["pid"])):
			push_warning("Meowody MCP: this project already has a live editor bridge; leaving its endpoint unchanged.")
			return
	_token = Crypto.new().generate_random_bytes(32).hex_encode()
	if _token.length() != 64:
		push_error("Meowody MCP: could not generate the session token.")
		return
	var err := _tcp_server.listen(0, "127.0.0.1")
	if err != OK:
		push_error("Meowody MCP: could not start the loopback bridge (error %d)." % err)
		return
	var directory := ProjectSettings.globalize_path(ENDPOINT.get_base_dir())
	err = DirAccess.make_dir_recursive_absolute(directory)
	var file := FileAccess.open(ENDPOINT, FileAccess.WRITE) if err == OK else null
	if file == null:
		_tcp_server.stop()
		push_error("Meowody MCP: could not write the editor endpoint.")
		return
	file.store_string(JSON.stringify({
		"protocol": 1, "project_path": _project_path, "pid": OS.get_process_id(),
		"url": "ws://127.0.0.1:%d" % _tcp_server.get_local_port(), "token": _token,
	}))
	file.close()
	_alive = true
	print("Meowody MCP: editor bridge ready for this project.")

func _exit_tree() -> void:
	_alive = false
	for ws in _peers:
		ws.close()
	_peers.clear()
	_tcp_server.stop()
	if FileAccess.file_exists(ENDPOINT):
		var endpoint = JSON.parse_string(FileAccess.get_file_as_string(ENDPOINT))
		if endpoint is Dictionary and not _token.is_empty() and endpoint.get("token") == _token:
			DirAccess.remove_absolute(ProjectSettings.globalize_path(ENDPOINT))

func _process(_delta: float) -> void:
	if not _alive:
		return
	while _tcp_server.is_connection_available():
		var tcp := _tcp_server.take_connection()
		if _peers.size() >= 8:
			tcp.disconnect_from_host()
			continue
		var ws := WebSocketPeer.new()
		ws.inbound_buffer_size = 1024 * 1024
		ws.outbound_buffer_size = 16 * 1024 * 1024
		ws.max_queued_packets = 64
		if ws.accept_stream(tcp) == OK:
			_peers.append(ws)
	for i in range(_peers.size() - 1, -1, -1):
		var ws := _peers[i]
		ws.poll()
		if ws.get_ready_state() == WebSocketPeer.STATE_OPEN:
			for _packet in 16:
				if ws.get_available_packet_count() == 0:
					break
				_handle_message(ws, ws.get_packet().get_string_from_utf8())
		elif ws.get_ready_state() == WebSocketPeer.STATE_CLOSED:
			_peers.remove_at(i)

func _handle_message(ws: WebSocketPeer, raw: String) -> void:
	var data = JSON.parse_string(raw)
	if not data is Dictionary:
		_reply(ws, null, _fail("Invalid JSON request."))
		return
	var id = data.get("id")
	if data.get("token") != _token or _normalize_path(str(data.get("project_path", ""))) != _project_path:
		_reply(ws, id, _fail("Editor session authentication or project identity did not match."))
		ws.close(1008, "Wrong editor session")
		return
	var args = data.get("args", {})
	if not args is Dictionary:
		_reply(ws, id, _fail("args must be an object."))
		return
	var command := str(data.get("command", ""))
	if command in MUTATIONS and (_opening_scene or _binding_busy()):
		_reply(ws, id, _fail("The editor is opening a scene or building/reloading a Meowody binding. Try again when it finishes."))
		return
	var out: Dictionary = await _dispatch(command, args)
	_reply(ws, id, out)

func _dispatch(command: String, args: Dictionary) -> Dictionary:
	match command:
		"ping", "status":
			return _ok({"status": "alive", "editor": "meowody_mcp", "project_path": _project_path,
				"current_scene": _current_scene_path(), "playing": EditorInterface.is_playing_scene(),
				"godot_version": Engine.get_version_info(), "binding_busy": _binding_busy()})
		"get_scene_tree":
			return _cmd_get_scene_tree()
		"get_open_scenes":
			return _ok({"project_path": _project_path, "open": EditorInterface.get_open_scenes(), "current": _current_scene_path()})
		"get_selection":
			var root := EditorInterface.get_edited_scene_root()
			var selected: Array[String] = []
			for node in EditorInterface.get_selection().get_selected_nodes():
				if root and (node == root or root.is_ancestor_of(node)):
					selected.append(str(root.get_path_to(node)))
			return _ok({"selected": selected})
		"get_node_properties":
			return _cmd_get_node_properties(args)
		"select_node":
			var root := EditorInterface.get_edited_scene_root()
			var path := str(args.get("node_path", "."))
			var node := _find_node(root, path)
			if node == null:
				return _fail("Node is not part of the editable scene: " + path)
			_select_only(node)
			return _ok({"selected": [str(root.get_path_to(node))]})
		"open_scene":
			return await _cmd_open_scene(args)
		"add_node":
			return _cmd_add_node(args)
		"set_properties":
			return _cmd_set_properties(args)
		"remove_node":
			return _cmd_remove_node(args)
		"save_scene":
			return _cmd_save_scene()
		"run_scene":
			var path := _to_res_path(str(args.get("scene_path", "")))
			if path.is_empty() or not ResourceLoader.exists(path) or not (ResourceLoader.load(path) is PackedScene):
				return _fail("scene_path must reference a project scene.")
			EditorInterface.play_custom_scene(path)
			return _ok({"playing": path})
		"stop":
			EditorInterface.stop_playing_scene()
			return _ok({"stopped": true})
		"undo", "redo":
			return _cmd_history(command)
		"get_editor_output":
			return _cmd_get_editor_output(args)
		"screenshot":
			return await _cmd_screenshot(args)
		_:
			return _fail("Unknown command: " + command)

func _cmd_get_scene_tree() -> Dictionary:
	var root := EditorInterface.get_edited_scene_root()
	if root == null:
		return _fail("No scene is open; use editor_open_scene first.")
	return _ok({"scene": root.scene_file_path, "open_scenes": EditorInterface.get_open_scenes(), "tree": _describe(root, root)})

func _cmd_open_scene(args: Dictionary) -> Dictionary:
	var path := _to_res_path(str(args.get("scene_path", "")))
	if path.is_empty() or not ResourceLoader.exists(path) or not (ResourceLoader.load(path) is PackedScene):
		return _fail("scene_path must reference a project scene.")
	_opening_scene = true
	EditorInterface.open_scene_from_path(path)
	var deadline := Time.get_ticks_msec() + 6000
	while _alive:
		await get_tree().process_frame
		if _current_scene_path() == path:
			_opening_scene = false
			return _ok({"opened": path})
		if Time.get_ticks_msec() >= deadline:
			break
	_opening_scene = false
	return _fail("The editor did not finish opening the scene.")

func _cmd_add_node(args: Dictionary) -> Dictionary:
	var scene_err := _scene_error(args)
	if not scene_err.is_empty():
		return _fail(scene_err)
	var root := EditorInterface.get_edited_scene_root()
	var parent_path := str(args.get("parent_path", "."))
	var parent := _find_node(root, parent_path)
	var node_name := str(args.get("name", ""))
	var node_type := str(args.get("type", ""))
	if parent == null:
		return _fail("Parent is not part of the editable scene: " + parent_path)
	if node_name.is_empty() or _has_bad_chars(node_name):
		return _fail("Invalid node name: " + node_name)
	if parent.has_node(NodePath(node_name)):
		return _fail("A child already has this name: " + node_name)
	if not (ClassDB.class_exists(node_type) and ClassDB.is_parent_class(node_type, "Node") and ClassDB.can_instantiate(node_type)):
		return _fail("Not an instantiable Node class: " + node_type)
	var node := ClassDB.instantiate(node_type) as Node
	node.name = node_name
	var script_path := str(args.get("script_path", ""))
	if not script_path.is_empty():
		var path := _to_res_path(script_path)
		var script = ResourceLoader.load(path) if not path.is_empty() else null
		if not script is Script or not node.is_class(script.get_instance_base_type()):
			node.free()
			return _fail("Script is missing or incompatible with the node: " + script_path)
		node.set_script(script)
	var properties = args.get("properties", {})
	var prepared := _prepare_properties(node, properties, root)
	if not prepared["error"].is_empty():
		node.free()
		return _fail(prepared["error"])
	for change in prepared["changes"]:
		change["target"].set(change["key"], change["value"])
	var ur := get_undo_redo()
	ur.create_action("MCP: add " + node_name, UndoRedo.MERGE_DISABLE, root)
	ur.add_do_method(parent, "add_child", node, true)
	ur.add_do_method(node, "set_owner", root)
	ur.add_do_reference(node)
	ur.add_undo_method(parent, "remove_child", node)
	ur.commit_action()
	_select_only(node)
	return _ok({"added": str(root.get_path_to(node)), "type": node_type})

func _cmd_set_properties(args: Dictionary) -> Dictionary:
	var scene_err := _scene_error(args)
	if not scene_err.is_empty():
		return _fail(scene_err)
	var root := EditorInterface.get_edited_scene_root()
	var node_path := str(args.get("node_path", "."))
	var node := _find_node(root, node_path)
	if node == null:
		return _fail("Node is not part of the editable scene: " + node_path)
	var properties = args.get("properties", {})
	if not properties is Dictionary or properties.is_empty():
		return _fail("properties must be a nonempty object.")
	var prepared := _prepare_properties(node, properties, root)
	if not prepared["error"].is_empty():
		return _fail(prepared["error"])
	var ur := get_undo_redo()
	ur.create_action("MCP: set properties on %s" % node.name, UndoRedo.MERGE_DISABLE, root)
	for change in prepared["changes"]:
		ur.add_do_property(change["target"], change["key"], change["value"])
		ur.add_undo_property(change["target"], change["key"], change["old_value"])
	ur.commit_action()
	_select_only(node)
	return _ok({"updated": node_path, "properties": properties.keys()})

func _cmd_remove_node(args: Dictionary) -> Dictionary:
	var scene_err := _scene_error(args)
	if not scene_err.is_empty():
		return _fail(scene_err)
	var root := EditorInterface.get_edited_scene_root()
	var node_path := str(args.get("node_path", ""))
	var node := _find_node(root, node_path)
	if node == null or node == root:
		return _fail("Cannot remove the scene root or a node outside the editable scene.")
	var parent := node.get_parent()
	var index := node.get_index()
	var ur := get_undo_redo()
	ur.create_action("MCP: remove %s" % node.name, UndoRedo.MERGE_DISABLE, root)
	ur.add_do_method(parent, "remove_child", node)
	ur.add_undo_method(parent, "add_child", node, true)
	ur.add_undo_method(parent, "move_child", node, index)
	for owned in _owned_nodes(node, root):
		ur.add_undo_method(owned, "set_owner", root)
	ur.add_undo_reference(node)
	ur.commit_action()
	return _ok({"removed": node_path})

func _cmd_save_scene() -> Dictionary:
	var root := EditorInterface.get_edited_scene_root()
	if root == null or _to_res_path(root.scene_file_path).is_empty():
		return _fail("Save this scene once inside the project before using MCP save.")
	var err := EditorInterface.save_scene()
	return _ok({"saved": root.scene_file_path}) if err == OK else _fail("Scene save failed (error %d)." % err)

func _cmd_history(command: String) -> Dictionary:
	var root := EditorInterface.get_edited_scene_root()
	if root == null:
		return _fail("No scene is open.")
	var manager := get_undo_redo()
	var history := manager.get_history_undo_redo(manager.get_object_history_id(root))
	if history == null or (not history.has_undo() if command == "undo" else not history.has_redo()):
		return _fail("Nothing to %s in the current scene." % command)
	history.undo() if command == "undo" else history.redo()
	return _ok({"action": command, "scene": root.scene_file_path})

func _cmd_get_node_properties(args: Dictionary) -> Dictionary:
	var root := EditorInterface.get_edited_scene_root()
	var node_path := str(args.get("node_path", "."))
	var node := _find_node(root, node_path)
	if node == null:
		return _fail("Node is not part of the editable scene: " + node_path)
	var requested = args.get("properties", [])
	if not requested is Array:
		return _fail("properties must be an array of property names.")
	var values := {}
	if requested.is_empty():
		for info in node.get_property_list():
			if info["usage"] & PROPERTY_USAGE_STORAGE and not str(info["name"]).begins_with("_"):
				values[info["name"]] = _json_value(node.get(info["name"]), root)
	else:
		for key in requested:
			var target := _property_target(node, str(key), {})
			if target.is_empty():
				return _fail("Property does not exist: " + str(key))
			values[str(key)] = _json_value(target["target"].get(target["key"]), root)
	return _ok({"node_path": node_path, "class_name": node.get_class(), "values": values})

func _cmd_get_editor_output(args: Dictionary) -> Dictionary:
	var limit := clampi(int(args.get("lines", 100)), 1, 1000)
	var stack: Array[Node] = [EditorInterface.get_base_control()]
	while not stack.is_empty():
		var current := stack.pop_back() as Node
		if current.get_class() == "EditorLog":
			var labels := current.find_children("*", "RichTextLabel", true, false)
			if not labels.is_empty():
				var output := (labels[0] as RichTextLabel).get_parsed_text().split("\n")
				return _ok({"source": "editor_output", "lines": output.slice(maxi(0, output.size() - limit)), "truncated": output.size() > limit})
		for child in current.get_children(true):
			stack.append(child)
	var path := str(ProjectSettings.get_setting("debug/file_logging/log_path", "user://logs/godot.log"))
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return _fail("No file log is available. Enable debug/file_logging/enable_file_logging or use the editor Output panel.")
	var length := file.get_length()
	var start := maxi(0, length - 256 * 1024)
	file.seek(start)
	var lines := file.get_buffer(length - start).get_string_from_utf8().split("\n")
	file.close()
	if start > 0 and not lines.is_empty():
		lines.remove_at(0)
	var truncated := start > 0 or lines.size() > limit
	var recent := lines.slice(maxi(0, lines.size() - limit))
	return _ok({"source": path, "lines": recent, "truncated": truncated})

func _cmd_screenshot(args: Dictionary) -> Dictionary:
	if DisplayServer.get_name() == "headless":
		return _fail("Viewport screenshots require a graphical editor.")
	var kind := str(args.get("viewport", "2d"))
	if kind not in ["2d", "3d"]:
		return _fail("viewport must be '2d' or '3d'.")
	var viewport := EditorInterface.get_editor_viewport_2d() if kind == "2d" else EditorInterface.get_editor_viewport_3d()
	if viewport == null:
		return _fail("The editor viewport is unavailable.")
	await RenderingServer.frame_post_draw
	if not _alive or not is_instance_valid(viewport):
		return _fail("The editor bridge or viewport was closed.")
	var image := viewport.get_texture().get_image()
	if image == null or image.is_empty():
		return _fail("The editor viewport has no rendered image.")
	return _ok({"mime_type": "image/png", "data": Marshalls.raw_to_base64(image.save_png_to_buffer())})

func _prepare_properties(node: Object, properties, root: Node) -> Dictionary:
	var out := {"changes": [], "error": ""}
	if not properties is Dictionary:
		out["error"] = "properties must be an object."
		return out
	var staged := {}
	for key in properties:
		var target := _property_target(node, str(key), staged)
		var parsed := _parse_value(str(properties[key]), root)
		if target.is_empty():
			out["error"] = "Property does not exist: " + str(key)
			return out
		if not parsed["error"].is_empty():
			out["error"] = "Property %s: %s" % [key, parsed["error"]]
			return out
		var value_error := _validate_value(target["target"], target["key"], parsed["value"])
		if not value_error.is_empty():
			out["error"] = "Property %s: %s" % [key, value_error]
			return out
		out["changes"].append({"target": target["target"], "key": target["key"], "value": parsed["value"], "old_value": target["target"].get(target["key"])})
		staged[str(key)] = parsed["value"]
	return out

func _property_target(node: Object, key: String, staged: Dictionary) -> Dictionary:
	var parts := key.split(":")
	var target := node
	var prefix := ""
	for i in parts.size():
		if parts[i].is_empty() or not parts[i] in target:
			return {}
		if i == parts.size() - 1:
			return {"target": target, "key": parts[i]}
		prefix += (":" if i > 0 else "") + parts[i]
		var value = staged.get(prefix, target.get(parts[i]))
		if not value is Resource:
			return {}
		target = value
	return {}

func _validate_value(target: Object, key: String, value) -> String:
	for info in target.get_property_list():
		if str(info["name"]) != key:
			continue
		if info["usage"] & PROPERTY_USAGE_READ_ONLY:
			return "Property is read-only."
		var expected := int(info["type"])
		var actual := typeof(value)
		if expected == TYPE_OBJECT:
			if value == null:
				return ""
			if not value is Object:
				return "Expected an Object; use load('res://...') or node('path')."
			var class_name_hint := str(info.get("class_name", ""))
			if not class_name_hint.is_empty() and not value.is_class(class_name_hint):
				return "Expected " + class_name_hint + "."
			return ""
		if expected == TYPE_NIL or actual == expected or (expected in [TYPE_FLOAT, TYPE_INT] and actual in [TYPE_FLOAT, TYPE_INT]):
			return ""
		if expected in [TYPE_STRING_NAME, TYPE_NODE_PATH] and actual == TYPE_STRING:
			return ""
		return "Expected %s, got %s." % [type_string(expected), type_string(actual)]
	return "Property does not exist."

func _parse_value(text: String, root: Node) -> Dictionary:
	var s := text.strip_edges()
	if s.contains("Object("):
		return {"value": null, "error": "Object(...) values are not allowed."}
	for wrapper in ["load", "node"]:
		if s.begins_with(wrapper + "(") and s.ends_with(")"):
			var argument = str_to_var(s.substr(wrapper.length() + 1, s.length() - wrapper.length() - 2))
			if not argument is String:
				return {"value": null, "error": "Wrapper argument must be a quoted string."}
			var value
			if wrapper == "node":
				value = _find_node(root, argument)
			else:
				var path := _to_res_path(argument)
				value = ResourceLoader.load(path) if not path.is_empty() and ResourceLoader.exists(path) else null
			return {"value": value, "error": "" if value != null else "Requested project resource or scene node does not exist."}
	if s.ends_with(".new()"):
		var cls := s.trim_suffix(".new()")
		if ClassDB.class_exists(cls) and ClassDB.is_parent_class(cls, "Resource") and ClassDB.can_instantiate(cls):
			return {"value": ClassDB.instantiate(cls), "error": ""}
		return {"value": null, "error": "Not an instantiable Resource class: " + cls}
	var value = str_to_var(s)
	return {"value": value, "error": "" if value != null or s == "null" else "Invalid Godot value."}

func _to_res_path(path: String) -> String:
	if path.is_empty():
		return ""
	var relative := path.trim_prefix("res://")
	if relative.is_absolute_path() or relative.contains("\\") or relative.contains(":"):
		return ""
	var parent := "res://"
	for part in relative.split("/"):
		if part.is_empty() or part in [".", ".."]:
			return ""
		var dir := DirAccess.open(parent)
		if dir and dir.is_link(parent.path_join(part)):
			return ""
		parent = parent.path_join(part)
	return "res://" + relative

func _find_node(root: Node, path: String) -> Node:
	if root == null:
		return null
	if path in [".", ""]:
		return root
	if path.begins_with("/") or path.contains(":") or path.contains("\\"):
		return null
	for part in path.split("/"):
		if part.is_empty() or part in [".", ".."]:
			return null
	var node := root.get_node_or_null(NodePath(path))
	return node if node and root.is_ancestor_of(node) and node.owner == root else null

func _normalize_path(path: String) -> String:
	var normalized := path.replace("\\", "/").simplify_path().trim_suffix("/")
	return normalized.to_lower() if OS.get_name() == "Windows" else normalized

func _binding_busy() -> bool:
	var stack: Array[Node] = [get_tree().root]
	while not stack.is_empty():
		var node := stack.pop_back() as Node
		var script = node.get_script()
		if script is Script and script.resource_path == BINDING_SCRIPT:
			return int(node.get("stage")) != 0
		for child in node.get_children():
			stack.append(child)
	return false

func _select_only(node: Node) -> void:
	var selection := EditorInterface.get_selection()
	selection.clear()
	selection.add_node(node)

func _scene_error(args: Dictionary) -> String:
	var root := EditorInterface.get_edited_scene_root()
	if root == null:
		return "No scene is open; use editor_open_scene first."
	var wanted := str(args.get("scene_path", ""))
	if not wanted.is_empty() and _to_res_path(wanted) != root.scene_file_path:
		return "scene_path does not match the currently edited scene."
	return ""

func _has_bad_chars(value: String) -> bool:
	for c in BAD_NAME_CHARS:
		if value.contains(c):
			return true
	return false

func _current_scene_path() -> String:
	var root := EditorInterface.get_edited_scene_root()
	return root.scene_file_path if root else ""

func _owned_nodes(node: Node, root: Node) -> Array:
	var out: Array = []
	if node.owner == root:
		out.append(node)
	for child in node.get_children():
		out.append_array(_owned_nodes(child, root))
	return out

func _describe(node: Node, root: Node) -> Dictionary:
	var children: Array = []
	for child in node.get_children():
		if child.owner == root:
			children.append(_describe(child, root))
	return {"name": str(node.name), "type": node.get_class(), "path": str(root.get_path_to(node)), "children": children}

func _json_value(value, root: Node):
	if value == null or typeof(value) in [TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_STRING]:
		return value
	if value is Resource:
		return {"class_name": value.get_class(), "resource_path": value.resource_path}
	if value is Node:
		return {"class_name": value.get_class(), "node_path": str(root.get_path_to(value))} if value == root or root.is_ancestor_of(value) else null
	if value is Object:
		return {"class_name": value.get_class()}
	return var_to_str(value)

func _ok(result = null) -> Dictionary:
	return {"ok": true, "result": result, "error": null}

func _fail(message: String) -> Dictionary:
	return {"ok": false, "result": null, "error": message}

func _reply(ws: WebSocketPeer, id, out: Dictionary) -> void:
	if _alive and ws.get_ready_state() == WebSocketPeer.STATE_OPEN:
		ws.send_text(JSON.stringify({"id": id, "ok": out["ok"], "result": out["result"], "error": out["error"]}))
