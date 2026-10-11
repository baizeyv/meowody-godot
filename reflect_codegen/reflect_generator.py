import os
import re
import shutil
from pathlib import Path

VARIANT_TYPES = {
    "bool": "Variant::BOOL", "int": "Variant::INT", "int8_t": "Variant::INT",
    "uint8_t": "Variant::INT", "int16_t": "Variant::INT", "uint16_t": "Variant::INT",
    "int32_t": "Variant::INT", "uint32_t": "Variant::INT", "int64_t": "Variant::INT",
    "uint64_t": "Variant::INT", "short": "Variant::INT", "unsigned short": "Variant::INT",
    "unsigned int": "Variant::INT", "long long": "Variant::INT", "unsigned long long": "Variant::INT",
    "float": "Variant::FLOAT", "double": "Variant::FLOAT", "real_t": "Variant::FLOAT",
    "String": "Variant::STRING", "StringName": "Variant::STRING_NAME", "Vector2": "Variant::VECTOR2",
    "Vector2i": "Variant::VECTOR2I", "Rect2": "Variant::RECT2", "Rect2i": "Variant::RECT2I",
    "Vector3": "Variant::VECTOR3", "Vector3i": "Variant::VECTOR3I", "Transform2D": "Variant::TRANSFORM2D",
    "Vector4": "Variant::VECTOR4", "Vector4i": "Variant::VECTOR4I", "Plane": "Variant::PLANE",
    "Quaternion": "Variant::QUATERNION", "AABB": "Variant::AABB", "Basis": "Variant::BASIS",
    "Transform3D": "Variant::TRANSFORM3D", "Projection": "Variant::PROJECTION", "Color": "Variant::COLOR",
    "NodePath": "Variant::NODE_PATH", "RID": "Variant::RID", "Callable": "Variant::CALLABLE",
    "Signal": "Variant::SIGNAL", "Dictionary": "Variant::DICTIONARY", "Array": "Variant::ARRAY",
    "PackedByteArray": "Variant::PACKED_BYTE_ARRAY", "PackedInt32Array": "Variant::PACKED_INT32_ARRAY",
    "PackedInt64Array": "Variant::PACKED_INT64_ARRAY", "PackedFloat32Array": "Variant::PACKED_FLOAT32_ARRAY",
    "PackedFloat64Array": "Variant::PACKED_FLOAT64_ARRAY", "PackedStringArray": "Variant::PACKED_STRING_ARRAY",
    "PackedVector2Array": "Variant::PACKED_VECTOR2_ARRAY", "PackedVector3Array": "Variant::PACKED_VECTOR3_ARRAY",
    "PackedColorArray": "Variant::PACKED_COLOR_ARRAY", "PackedVector4Array": "Variant::PACKED_VECTOR4_ARRAY",
    "Variant": "Variant::NIL",
}


def normalize_type(value):
    value = re.sub(r"\b(const|class|struct)\s+", "", value)
    value = value.replace("&", "")
    value = re.sub(r"\s*\*\s*", "*", value)
    return value.replace("godot::", "").strip()


def bind_type(value):
    normalized = normalize_type(value)
    if normalized in VARIANT_TYPES:
        return VARIANT_TYPES[normalized], "", "PROPERTY_HINT_NONE", ""
    if normalized.endswith("*"):
        return "Variant::OBJECT", normalized[:-1].split("::")[-1].strip(), "PROPERTY_HINT_NONE", ""
    if normalized.startswith("Ref<") and normalized.endswith(">"):
        resource = normalized[4:-1].strip()
        return "Variant::OBJECT", resource.split("::")[-1], "PROPERTY_HINT_RESOURCE_TYPE", resource.split("::")[-1]
    return "", "", "PROPERTY_HINT_NONE", ""


def attrs(cursor):
    result = []
    for child in cursor.get_children():
        if child.kind.name == "ANNOTATE_ATTR":
            name, _, payload = child.spelling.partition("|")
            result.append((name, payload))
    return result


def has_attr(items, name):
    return any(item[0] == name for item in items)


def payload(items, name):
    result = {}
    for attr_name, raw in items:
        if attr_name != name:
            continue
        for token in raw.split("|"):
            if "=" in token:
                key, value = token.split("=", 1)
                result[key] = value
    return result


def cursor_location(cursor):
    return Path(cursor.location.file.name).resolve(), cursor.location.line


def qualified_name(cursor):
    names = [cursor.spelling] if cursor.spelling else []
    parent = cursor.semantic_parent
    while parent and parent.kind.name != "TRANSLATION_UNIT":
        if parent.spelling and parent.kind.name in ("NAMESPACE", "CLASS_DECL", "STRUCT_DECL"):
            names.append(parent.spelling)
        parent = parent.semantic_parent
    return "::".join(reversed(names))


def collect_method(cursor):
    path, line = cursor_location(cursor)
    return {
        "name": cursor.spelling, "return": cursor.result_type.spelling,
        "const": cursor.is_const_method(), "static": cursor.is_static_method(),
        "definition": cursor.is_definition(), "access": cursor.access_specifier.name.lower(),
        "file": path, "line": line, "attributes": attrs(cursor),
        "args": [{"name": arg.spelling, "type": arg.type.spelling} for arg in cursor.get_arguments()],
    }


def collect_field(cursor):
    path, line = cursor_location(cursor)
    return {"name": cursor.spelling, "type": cursor.type.spelling, "file": path, "line": line, "attributes": attrs(cursor)}


def collect_class(cursor, source_root):
    from clang.cindex import CursorKind

    fields, methods, base_name = [], [], ""
    for child in cursor.get_children():
        if child.kind == CursorKind.CXX_BASE_SPECIFIER and not base_name:
            base_name = child.type.spelling
        elif child.kind == CursorKind.FIELD_DECL:
            fields.append(collect_field(child))
        elif child.kind == CursorKind.CXX_METHOD:
            methods.append(collect_method(child))
    if not any(has_attr(item["attributes"], "gdclass_marker") for item in methods):
        return None
    path, line = cursor_location(cursor)
    return {
        "name": cursor.spelling, "qualified": qualified_name(cursor), "base": normalize_type(base_name),
        "file": path, "include": path.relative_to(source_root).as_posix(), "line": line,
        "fields": fields, "methods": methods,
    }


def visit_root(cursor, source_root, result):
    from clang.cindex import CursorKind

    if cursor.kind in (CursorKind.NAMESPACE, CursorKind.TRANSLATION_UNIT):
        for child in cursor.get_children():
            visit_root(child, source_root, result)
        return
    if cursor.kind not in (CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL) or not cursor.is_definition():
        return
    if not cursor.location.file:
        return
    path = Path(cursor.location.file.name).resolve()
    try:
        path.relative_to(source_root)
    except ValueError:
        return
    item = collect_class(cursor, source_root)
    if item:
        result.append(item)


def scan_headers(source_root, godot_cpp_root, build_root, headers):
    from clang.cindex import Config, Index

    library_path = os.environ.get("LIBCLANG_PATH")
    if not library_path:
        executable = shutil.which("clang")
        if executable:
            library_path = str(Path(executable).resolve().parent)
    if library_path and not Config.loaded:
        Config.set_library_path(library_path)
    include_dirs = [source_root, godot_cpp_root / "include", godot_cpp_root / "gdextension", godot_cpp_root / "gen" / "include", build_root / "godot-cpp" / "gen" / "include"]
    args = ["-x", "c++", "-std=c++20", "-DGDREFLECT_CODEGEN=1", "-DGDEXTENSION", "-DDEBUG_ENABLED", "-DNOMINMAX", "-DTYPED_METHOD_BIND", "-DWINDOWS_ENABLED", "-DWIN32", "-D_WINDOWS", "-D_HAS_EXCEPTIONS=0", "-fms-compatibility", "-fms-extensions"]
    args.extend("-I" + str(path) for path in include_dirs if path.exists())
    result = []
    index = Index.create()
    headers = sorted(headers)
    if not headers:
        return result
    # One translation unit shares Godot/STL declarations instead of parsing them per header.
    # Function bodies and diagnostics remain enabled; source locations still refer to headers.
    scan_path = str(build_root / "gdreflect_scan.cpp")
    contents = "\n".join('#include "' + Path(header).resolve().as_posix() + '"' for header in headers)
    unit = index.parse(scan_path, args=args, unsaved_files=[(scan_path, contents)])
    errors = [str(diagnostic) for diagnostic in unit.diagnostics if diagnostic.severity >= diagnostic.Error]
    if errors:
        raise RuntimeError("Cannot generate reflection from invalid C++:\n" + "\n".join(errors))
    for cursor in unit.cursor.get_children():
        visit_root(cursor, source_root, result)
    return result


def build_model(raw_classes):
    model = []
    for raw in raw_classes:
        by_method = {method["name"]: method for method in raw["methods"]}
        properties, methods, signals = [], [], []
        for field in raw["fields"]:
            field_attrs = field["attributes"]
            field_payload = payload(field_attrs, "gdproperty")
            field_name = field["name"]
            if not has_attr(field_attrs, "gdproperty"):
                continue
            name = field_payload.get("name", field_name)
            getter = field_payload.get("getter", "get_" + field_name)
            setter = field_payload.get("setter", "set_" + field_name)
            if any(prop["name"] == name for prop in properties):
                raise RuntimeError(f"duplicate property name '{name}' at {field['file']}:{field['line']}")
            for accessor, is_getter in [(getter, True), (setter, False)]:
                candidates = [method for method in raw["methods"] if method["name"] == accessor]
                explicit = "getter" in field_payload if is_getter else "setter" in field_payload
                if not candidates:
                    if explicit:
                        raise RuntimeError(f"missing accessor '{accessor}' at {field['file']}:{field['line']}")
                    continue
                method = candidates[0]
                valid = len(candidates) == 1 and not method["static"] and method["access"] == "public"
                if is_getter:
                    valid = valid and not method["args"] and normalize_type(method["return"]) == normalize_type(field["type"])
                else:
                    valid = valid and method["return"] == "void" and len(method["args"]) == 1 and normalize_type(method["args"][0]["type"]) == normalize_type(field["type"])
                if not valid:
                    raise RuntimeError(f"invalid accessor '{accessor}' at {method['file']}:{method['line']}")
            variant, class_name, hint, hint_string = bind_type(field["type"])
            field_type = field["type"]
            if not variant:
                raise RuntimeError(f"unsupported property type: {field['type']} at {field['file']}:{field['line']}")
            properties.append({"name": name, "field": field_name, "type": field_type, "variant": variant,
                "class_name": class_name, "hint": field_payload.get("hint", hint), "hint_string": field_payload.get("hint_string", hint_string),
                "usage": field_payload.get("usage", "PROPERTY_USAGE_DEFAULT"), "category": field_payload.get("category", ""),
                "group": field_payload.get("group", ""), "prefix": field_payload.get("prefix", ""),
                "subgroup": field_payload.get("subgroup", ""), "subgroup_prefix": field_payload.get("subgroup_prefix", ""),
                "getter": getter, "setter": setter, "generate_getter": getter not in by_method,
                "generate_setter": setter not in by_method})
        for method in raw["methods"]:
            method_attrs = method["attributes"]
            if has_attr(method_attrs, "gdmethod") or has_attr(method_attrs, "gdrpc"):
                if method["static"] or method["access"] != "public":
                    raise RuntimeError(f"reflected method must be public and non-static at {method['file']}:{method['line']}")
                if sum(item["name"] == method["name"] for item in raw["methods"]) != 1:
                    raise RuntimeError(f"reflected overload is ambiguous at {method['file']}:{method['line']}")
                methods.append({"name": method["name"], "args": method["args"], "rpc": payload(method_attrs, "gdrpc")})
            elif has_attr(method_attrs, "gdsignal"):
                if method["static"] or method["access"] != "public" or method["return"] != "void":
                    raise RuntimeError(f"signal must be a public non-static void method at {method['file']}:{method['line']}")
                if any(not bind_type(arg["type"])[0] for arg in method["args"]):
                    raise RuntimeError(f"unsupported signal argument at {method['file']}:{method['line']}")
                if payload(method_attrs, "gdsignal").get("emit") == "true" and method["definition"]:
                    raise RuntimeError(f"generated signal already has a body at {method['file']}:{method['line']}")
                if sum(item["name"] == method["name"] for item in raw["methods"]) != 1:
                    raise RuntimeError(f"signal overload is ambiguous at {method['file']}:{method['line']}")
                signals.append({"name": method["name"], "args": method["args"], "emit": payload(method_attrs, "gdsignal").get("emit") == "true"})
        model.append({**raw, "properties": properties, "methods": methods, "signals": signals,
                  "notification": "_notification" in by_method})
    return sorted(model, key=lambda item: item["qualified"])


def cpp_string(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def render_class(cls):
    lines = [f'#include "{cls["include"]}"', '#include <gdextension_interface.h>', '#include <godot_cpp/godot.hpp>', '#include <godot_cpp/core/class_db.hpp>', '#include "reflection/property_category.h"', "", "using namespace godot;", ""]
    if any(method["rpc"] for method in cls["methods"]):
        lines[3:3] = ['#include <godot_cpp/classes/multiplayer_api.hpp>', '#include <godot_cpp/classes/multiplayer_peer.hpp>', '#include <godot_cpp/variant/dictionary.hpp>']
    for prop in cls["properties"]:
        if prop["generate_getter"]:
            lines += [f'{prop["type"]} {cls["qualified"]}::{prop["getter"]}() const {{', f'    return {prop["field"]};', "}", ""]
        if prop["generate_setter"]:
            lines += [f'void {cls["qualified"]}::{prop["setter"]}({prop["type"]} p_value) {{', f'    {prop["field"]} = p_value;', "}", ""]
    for signal in cls["signals"]:
        if signal["emit"]:
            arguments = ", ".join(f'{arg["type"]} {arg["name"] or "arg" + str(index + 1)}' for index, arg in enumerate(signal["args"]))
            values = ", ".join(arg["name"] or "arg" + str(index + 1) for index, arg in enumerate(signal["args"]))
            lines += [f'void {cls["qualified"]}::{signal["name"]}({arguments}) {{', f'    emit_signal("{signal["name"]}"' + (", " + values if values else "") + ");", "}", ""]
    if any(method["rpc"] for method in cls["methods"]):
        lines += [f'void {cls["qualified"]}::_gdreflect_apply_rpc_config() {{']
        for method in cls["methods"]:
            if method["rpc"]:
                rpc = method["rpc"]
                lines += ['    {', '        Dictionary opts;', f'        opts["rpc_mode"] = {rpc.get("rpc_mode", "MultiplayerAPI::RPC_MODE_DISABLED")};', f'        opts["transfer_mode"] = {rpc.get("transfer_mode", "MultiplayerPeer::TRANSFER_MODE_RELIABLE")};', f'        opts["call_local"] = {rpc.get("call_local", "false")};', f'        opts["channel"] = {rpc.get("channel", "0")};', f'        rpc_config("{method["name"]}", opts);', '    }']
        lines += ["}", ""]
        if not cls["notification"]:
            lines += [f'void {cls["qualified"]}::_notification(int p_what) {{', '    if (p_what == Node::NOTIFICATION_READY) {', '        _gdreflect_apply_rpc_config();', '    }', '}', ""]
    lines.append(f'void {cls["qualified"]}::_bind_methods() {{')
    for method in cls["methods"]:
        names = "".join(f', "{arg["name"] or "arg" + str(index + 1)}"' for index, arg in enumerate(method["args"]))
        lines.append(f'    ClassDB::bind_method(D_METHOD("{method["name"]}"{names}), &{cls["qualified"]}::{method["name"]});')
    bound_methods = {method["name"] for method in cls["methods"]}
    current_category = current_group = current_prefix = current_subgroup = current_subgroup_prefix = ""
    for prop in cls["properties"]:
        if prop["category"] and prop["category"] != current_category:
            lines.append(f'    meowody::reflection::add_category(get_class_static(), "{cpp_string(prop["category"])}");')
            current_category = prop["category"]
            current_group = current_prefix = current_subgroup = current_subgroup_prefix = ""
        if prop["group"] and (prop["group"], prop["prefix"]) != (current_group, current_prefix):
            lines.append(f'    ADD_GROUP("{cpp_string(prop["group"])}", "{cpp_string(prop["prefix"])}");')
            current_group, current_prefix = prop["group"], prop["prefix"]
            current_subgroup = current_subgroup_prefix = ""
        if prop["subgroup"] and (prop["subgroup"], prop["subgroup_prefix"]) != (current_subgroup, current_subgroup_prefix):
            lines.append(f'    ADD_SUBGROUP("{cpp_string(prop["subgroup"])}", "{cpp_string(prop["subgroup_prefix"])}");')
            current_subgroup, current_subgroup_prefix = prop["subgroup"], prop["subgroup_prefix"]
        for accessor, arguments in [(prop["getter"], ""), (prop["setter"], ', "value"')]:
            if accessor not in bound_methods:
                lines.append(f'    ClassDB::bind_method(D_METHOD("{accessor}"{arguments}), &{cls["qualified"]}::{accessor});')
                bound_methods.add(accessor)
        info = f'PropertyInfo({prop["variant"]}, "{cpp_string(prop["name"])}"'
        if prop["hint"] != "PROPERTY_HINT_NONE" or prop["hint_string"] or prop["usage"] != "PROPERTY_USAGE_DEFAULT" or prop["class_name"]:
            info += f', {prop["hint"]}, "{cpp_string(prop["hint_string"])}", {prop["usage"]}' + (f', "{cpp_string(prop["class_name"])}"' if prop["class_name"] else "")
        lines.append(f'    ADD_PROPERTY({info}), "{prop["setter"]}", "{prop["getter"]}");')
    for signal in cls["signals"]:
        args = "".join(f', PropertyInfo({bind_type(arg["type"])[0]}, "{arg["name"]}")' for arg in signal["args"])
        lines.append(f'    ADD_SIGNAL(MethodInfo("{signal["name"]}"{args}));')
    lines += ["}", ""]
    return "\n".join(lines)


def write_if_changed(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8")


def generate_project(output_dir, build_root, source_root, godot_cpp_root, headers):
    classes = build_model(scan_headers(source_root, godot_cpp_root, build_root, headers))
    unique_classes = {}
    for cls in classes:
        unique_classes.setdefault(cls["qualified"], cls)
    classes = list(unique_classes.values())
    names = {}
    for cls in classes:
        if cls["name"] in names:
            raise RuntimeError(f"duplicate Godot class name '{cls['name']}' at {cls['file']}:{cls['line']}; also declared in {names[cls['name']]}")
        names[cls["name"]] = cls["file"]
    output_dir.mkdir(parents=True, exist_ok=True)
    class_dir = output_dir / "classes"
    class_files = []
    for cls in classes:
        filename = cls["name"] + ".gdreflect.gen.cpp"
        class_files.append(filename)
        write_if_changed(class_dir / filename, render_class(cls))
    catalog = "#pragma once\n#include <map>\n#include <string>\nnamespace meowody { struct BindingTypeInfo { std::string cpp, header; };\ninline const std::map<std::string, BindingTypeInfo> binding_type_catalog = {\n"
    for cls in classes:
        catalog += f'{{"{cpp_string(cls["name"])}",{{"{cpp_string(cls["qualified"])}","{cpp_string(cls["include"])}"}}}},\n'
    write_if_changed(output_dir / "binding_type_catalog.gen.h", catalog + "}; }\n")
    declarations = "#pragma once\n\n#define GDREFLECT_PP_CAT_INNER(a, b) a##b\n#define GDREFLECT_PP_CAT(a, b) GDREFLECT_PP_CAT_INNER(a, b)\n#define GDREFLECT_CLASS_GENERATED_MEMBERS(ClassName) GDREFLECT_PP_CAT(GDREFLECT_GENERATED_MEMBERS_, ClassName)\n\n"
    for cls in classes:
        members = []
        for prop in cls["properties"]:
            if prop["generate_getter"]:
                members.append(f'{prop["type"]} {prop["getter"]}() const;')
            if prop["generate_setter"]:
                members.append(f'void {prop["setter"]}({prop["type"]} p_value);')
        if any(method["rpc"] for method in cls["methods"]):
            members.append('void _gdreflect_apply_rpc_config();')
            if not cls["notification"]:
                members.append('void _notification(int p_what);')
        if not members:
            declarations += f'#define GDREFLECT_GENERATED_MEMBERS_{cls["name"]}\n\n'
        else:
            declarations += f'#define GDREFLECT_GENERATED_MEMBERS_{cls["name"]} \\\n' + " \\\n".join(f'    {member}' for member in members) + "\n\n"
    write_if_changed(output_dir / "gdreflect_decls.gen.h", declarations)
    includes = "\n".join(f'#include "{cls["include"]}"' for cls in classes)
    pending = list(classes)
    ordered = []
    while pending:
        names = {item["qualified"] for item in pending} | {item["name"] for item in pending}
        ready = [item for item in pending if item["base"] not in names]
        if not ready:
            raise RuntimeError("cyclic reflected class inheritance")
        ordered.extend(ready)
        pending = [item for item in pending if item not in ready]
    registrations = "\n".join(f'    ClassDB::register_class<{cls["qualified"]}>();' for cls in ordered)
    write_if_changed(output_dir / "gd_register.gen.cpp", f'#include "register_types.h"\n#include <godot_cpp/core/class_db.hpp>\n{includes}\n\nusing namespace godot;\n\nvoid register_generated_types() {{\n{registrations}\n}}\n')
    write_if_changed(output_dir / "gdreflect_bindings.gen.cpp", "\n".join(f'#include "classes/{name}"' for name in class_files) + "\n")
