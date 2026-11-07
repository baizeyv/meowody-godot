#include "project_template.h"
#include "creator_backend.h"
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <fstream>
#include <algorithm>
#include <regex>

namespace creator {
using namespace godot;
namespace fs = std::filesystem;
namespace {
std::string bytes(const String &text) { return text.utf8().get_data(); }
String text(const std::string &value) { return String::utf8(value.c_str()); }
String read(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    std::string value{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return text(value);
}
bool write(const fs::path &path, const String &value, std::string &error) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) { error = ec.message(); return false; }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << bytes(value);
    if (!file) error = "Cannot write " + utf8(path);
    return bool(file);
}
Dictionary failure(const std::string &message) { Dictionary result; result["ok"] = false; result["message"] = text(message); return result; }
}
Dictionary generate_project(const Dictionary &request) {
    String name = request.get("name", ""), source_text = request.get("source", ""), destination_text = request.get("destination", "");
    String api = request.get("api_version", ""), version = request.get("engine_version", ""), commit = request.get("framework_commit", "");
    if (name.strip_edges().is_empty() || source_text.is_empty() || destination_text.is_empty()) return failure("Enter a project name and destination.");
    for (int64_t i = 0; i < name.length(); ++i) if (name[i] < 32) return failure("Project name contains control characters.");
    if (!std::regex_match(bytes(api), std::regex(R"(4\.\d+)"))) return failure("Invalid engine API version.");
    std::error_code ec;
    auto source = fs::canonical(fs::u8path(bytes(source_text)), ec);
    if (ec) return failure(ec.message());
    auto destination = fs::weakly_canonical(fs::u8path(bytes(destination_text)), ec);
    if (ec) return failure(ec.message());
    String normalized_source = text(utf8(source)).to_lower(), normalized_destination = text(utf8(destination)).to_lower();
    if (normalized_destination == normalized_source || normalized_destination.begins_with(normalized_source + String("/"))) return failure("Create the new project outside its framework checkout.");
    if (!empty_destination(utf8(destination))) return failure("Destination must be a new or empty directory; nothing will be overwritten.");
    Variant parsed = JSON::parse_string(read(source / "framework-dependencies.json"));
    if (parsed.get_type() != Variant::DICTIONARY) return failure("Framework revision lacks a valid framework-dependencies.json.");
    Dictionary dependencies = parsed;
    Variant item = dependencies.get("godot_cpp", Variant());
    if (item.get_type() != Variant::DICTIONARY) return failure("Framework must pin official godot-cpp.");
    Dictionary dependency = item;
    String pin = request.get("godot_cpp_commit", dependency.get("commit", ""));
    if (String(dependency.get("url", "")) != "https://github.com/godotengine/godot-cpp.git" || !std::regex_match(bytes(pin), std::regex("[0-9a-f]{40}"))) return failure("Framework must pin an official godot-cpp commit.");
    std::string error;
    for (const auto *directory : {"runtime", "editor", "log", "reflection"})
        if (!copy_source(source / "src" / directory, destination / "src" / directory, error)) return failure(error);
    for (const auto *filename : {"register_types.cpp", "register_types.h", "editor_registration.h", "editor_registration_stub.cpp"}) {
        fs::copy_file(source / "src" / filename, destination / "src" / filename, fs::copy_options::overwrite_existing, ec);
        if (ec) return failure(ec.message());
    }
    for (const auto *directory : {"reflect_codegen", "tools/godot_mcp", ".agents/skills/meowody-framework"})
        if (!copy_source(source / directory, destination / directory, error)) return failure(error);
    for (const auto *filename : {"SConstruct", ".gitignore", ".clang-format", "framework-dependencies.json"}) {
        fs::copy_file(source / filename, destination / filename, fs::copy_options::overwrite_existing, ec);
        if (ec) return failure(ec.message());
    }
    dependency["commit"] = pin;
    dependency["tag"] = request.get("godot_cpp_tag", dependency.get("tag", ""));
    Array target_apis; target_apis.push_back(api); dependency["api_versions"] = target_apis;
    dependencies["godot_cpp"] = dependency;
    if (!write(destination / "framework-dependencies.json", JSON::stringify(dependencies, "  "), error)) return failure(error);
    fs::create_directories(destination / "src/ui/generated", ec);
    if (ec) return failure(ec.message());
    if (!write(destination / "FRAMEWORK.md", read(source / "README.md").replace("meow/", "project/").replace("--path meow", "--path project"), error)) return failure(error);
    auto skill = destination / ".agents/skills/meowody-framework/SKILL.md";
    if (!write(skill, read(skill).replace("meow/", "project/").replace("Root README.md is the single handbook", "FRAMEWORK.md is the framework handbook"), error)) return failure(error);
    auto project = destination / "project";
    if (!copy_source(source / "meow/addons", project / "addons", error)) return failure(error);
    auto settings_path = source / "meow/resources/settings.json";
    if (fs::is_regular_file(settings_path, ec)) {
        Variant resource_settings = JSON::parse_string(read(settings_path));
        if (resource_settings.get_type() != Variant::DICTIONARY) return failure("Invalid framework resource settings.");
        Dictionary settings = resource_settings;
        settings["entries"] = Array();
        Dictionary local; local["name"] = "Local"; local["remote"] = false; local["startup"] = true;
        Array groups; groups.push_back(local); settings["groups"] = groups;
        if (!write(project / "resources/settings.json", JSON::stringify(settings, "  "), error)) return failure(error);
    }
    std::string extension = bytes(read(source / "meow/bin/meowody.gdextension"));
    if (extension.empty()) return failure("Missing framework extension descriptor.");
    extension = std::regex_replace(extension, std::regex("compatibility_minimum\\s*=\\s*\"[^\"]+\""), "compatibility_minimum = \"" + bytes(api) + "\"");
    if (!write(project / "bin/meowody.gdextension", text(extension), error)) return failure(error);
    std::vector<std::string> plugins;
    for (fs::directory_iterator iterator(project / "addons", ec), end; !ec && iterator != end; iterator.increment(ec))
        if (fs::is_regular_file(iterator->path() / "plugin.cfg", ec)) plugins.push_back("res://addons/" + utf8(iterator->path().filename()) + "/plugin.cfg");
    if (ec) return failure(ec.message());
    std::sort(plugins.begin(), plugins.end());
    String plugin_values;
    for (const auto &plugin : plugins) { if (!plugin_values.is_empty()) plugin_values += ", "; plugin_values += JSON::stringify(text(plugin)); }
    String config = String("config_version=5\n[application]\nconfig/name=") + JSON::stringify(name) + String("\nrun/main_scene=\"res://main.tscn\"\nconfig/features=PackedStringArray(\"") + api + String("\", \"GL Compatibility\")\n[editor_plugins]\nenabled=PackedStringArray(") + plugin_values + String(")\n[rendering]\nrenderer/rendering_method=\"gl_compatibility\"\ntextures/vram_compression/import_etc2_astc=true\n");
    if (!write(project / "project.godot", config, error) || !write(project / "main.tscn", "[gd_scene format=3]\n\n[node name=\"Main\" type=\"Node\"]\n", error)) return failure(error);
    Dictionary metadata;
    metadata["project_dir"] = "project"; metadata["api_version"] = api; metadata["custom_api_file"] = "godot-api/extension_api.json";
    metadata["godot_version"] = version; metadata["framework_url"] = "https://github.com/baizeyv/meowody-godot.git";
    metadata["framework_commit"] = commit; metadata["godot_cpp_commit"] = pin;
    metadata["godot_cpp_tag"] = dependency["tag"];
    if (!write(destination / "meowody-project.json", JSON::stringify(metadata, "  "), error)) return failure(error);
    String instructions = String("# ") + name + String("\n\nOpen project/project.godot. Framework reference: FRAMEWORK.md.\nBuild Debug with `scons`; Release with `scons target=template_release`.\nInstall Git, Python, SCons, matching clang bindings/libclang, MSVC and a Windows SDK.\nAfter cloning, restore dependencies with `git submodule update --init --recursive`.\nCommit .gitmodules, the godot-cpp gitlink and framework-dependencies.json together.\nNo commits or pushes are made automatically.\nMCP server: `cd tools/godot_mcp`, `npm ci --ignore-scripts`, `npm run build`.\nStart with `node tools/godot_mcp/dist/index.js --project ABSOLUTE_PATH/project`.\n");
    if (!write(destination / "README.md", instructions, error)) return failure(error);
    Dictionary result; result["ok"] = true; result["pin"] = pin; return result;
}
}
