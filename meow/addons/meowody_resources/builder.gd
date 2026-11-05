@tool
extends RefCounted
const Settings = preload("settings.gd")

static func dependencies(path: String, output: Dictionary) -> void:
	if output.has(path): return
	output[path] = true
	for dependency in ResourceLoader.get_dependencies(path):
		var parts := dependency.split("::")
		var target: String = parts[-1]
		if parts.size() == 1 and target.begins_with("uid://"):
			var uid := ResourceUID.text_to_id(target)
			if ResourceUID.has_id(uid): target = ResourceUID.get_id_path(uid)
		if target.begins_with("res://"): dependencies(target, output)

static func is_code(path: String) -> bool:
	return path.get_extension() in ["gd", "gdshader", "gdshaderinc", "gdextension", "dll", "so", "dylib"]

static func analyze(settings: Dictionary) -> Dictionary:
	var errors := Settings.resolve(settings)
	if not errors.is_empty(): return {"errors": Array(errors)}
	var groups := {}
	var owners := {}
	var closures := {}
	var boot := {}
	var main_scene := str(ProjectSettings.get_setting("application/run/main_scene", ""))
	if main_scene.begins_with("uid://"):
		main_scene = ResourceUID.get_id_path(ResourceUID.text_to_id(main_scene))
	if not main_scene.is_empty(): dependencies(main_scene, boot)
	for property in ProjectSettings.get_property_list():
		if str(property.name).begins_with("autoload/"):
			dependencies(str(ProjectSettings.get_setting(property.name)).trim_prefix("*"), boot)
	for group in settings.groups: groups[group.name] = group
	for entry in settings.entries:
		if is_code(entry.path) and groups[entry.group].get("remote", false): errors.append("Remote entries cannot contain code: " + entry.path)
		var found := {}
		dependencies(entry.path, found)
		closures[entry.address] = found
		for path in found:
			if is_code(path): continue
			if not owners.has(path): owners[path] = []
			if entry.group not in owners[path]: owners[path].append(entry.group)
	# Boot content stays in the base application; direct remote references are invalid.
	for entry in settings.entries:
		if groups[entry.group].get("remote", false):
			if boot.has(entry.path): errors.append("Startup content references remote resource: " + entry.path)
		else:
			for other in settings.entries:
				if other.group != entry.group and groups[other.group].get("remote", false) and closures[entry.address].has(other.path):
					errors.append("Local resource directly references a remote entry: " + other.path)
	var assignments := {}
	var packs := {}
	for path in owners:
		if boot.has(path): continue
		var use: Array = owners[path]
		use.sort()
		var id := "group_" + str(use[0]).sha256_text().substr(0, 12) if use.size() == 1 else "shared_" + JSON.stringify(use).sha256_text().substr(0, 12)
		assignments[path] = id
		if not packs.has(id): packs[id] = {"files": [], "groups": use, "dependencies": [], "remote": true, "startup": false}
		packs[id].files.append(path)
		for group_name in use:
			packs[id].remote = packs[id].remote and groups[group_name].get("remote", false)
			packs[id].startup = packs[id].startup or groups[group_name].get("startup", false)
	for entry in settings.entries:
		if not groups[entry.group].get("remote", false):
			for path in closures[entry.address]:
				for owner in owners.get(path, []):
					if groups[owner].get("remote", false) and path != entry.path:
						# Sharing a dependency is valid: its generated package becomes local.
						if assignments.has(path): packs[assignments[path]].remote = false
	var edges := {}
	for path in assignments:
		var found := {}
		dependencies(path, found)
		var id: String = assignments[path]
		if not edges.has(id): edges[id] = {}
		for dependency in found:
			if assignments.has(dependency) and assignments[dependency] != id: edges[id][assignments[dependency]] = true
	# Collapse strongly connected package sets by mutual reachability.
	var reach := {}
	for id in packs:
		reach[id] = {}
		var queue: Array = edges.get(id, {}).keys()
		while not queue.is_empty():
			var next: String = queue.pop_back()
			if reach[id].has(next): continue
			reach[id][next] = true
			queue.append_array(edges.get(next, {}).keys())
	var merged := {}
	var notes := []
	for id in packs:
		var component := [id]
		for other in packs:
			if other != id and reach[id].has(other) and reach[other].has(id): component.append(other)
		component.sort()
		merged[id] = component[0] if component.size() == 1 else "shared_cycle_" + JSON.stringify(component).sha256_text().substr(0, 12)
		if component.size() > 1 and id == component[0]: notes.append({"merged": component, "pack": merged[id]})
	var collapsed := {}
	for id in packs:
		var target: String = merged[id]
		if not collapsed.has(target): collapsed[target] = {"files": [], "groups": [], "dependencies": [], "remote": true, "startup": false}
		var pack: Dictionary = collapsed[target]
		pack.files.append_array(packs[id].files)
		pack.remote = pack.remote and packs[id].remote
		pack.startup = pack.startup or packs[id].startup
		for group in packs[id].groups:
			if group not in pack.groups: pack.groups.append(group)
		for dep in edges.get(id, {}):
			if merged[dep] != target and merged[dep] not in pack.dependencies: pack.dependencies.append(merged[dep])
	for path in assignments: assignments[path] = merged[assignments[path]]
	for id in collapsed:
		collapsed[id].files.sort()
		collapsed[id].dependencies.sort()
	var entries := []
	for item in settings.entries:
		var entry: Dictionary = item.duplicate(true)
		entry.pack = assignments.get(entry.path, "")
		entries.append(entry)
	return {"errors": Array(errors), "packs": collapsed, "entries": entries, "boot": boot.keys(), "cycles": notes, "entry_dependencies": closures}
