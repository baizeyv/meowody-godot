extends SceneTree

var failures := 0

func check(value: bool, message: String):
    if not value:
        failures += 1
        push_error(message)

func _initialize():
    call_deferred("run")

func run():
    var args = OS.get_cmdline_user_args()
    var source = args[args.find("--source") + 1]
    var destination = args[args.find("--destination") + 1]
    var version = args[args.find("--version") + 1]
    var view = ClassDB.instantiate("ProjectCreatorView")
    var request = {"source": source, "destination": destination, "name": '原生项目 "Test"', "engine_version": version,
        "api_version": "4." + version.get_slice(".", 1), "framework_commit": "native-validation"}
    var result = view.generate_project(request)
    check(result.get("ok", false), "Native template generation failed: " + str(result))
    if not result.get("ok", false):
        view.free()
        quit(1)
        return
    check(FileAccess.file_exists(destination.path_join("project/main.tscn")), "Empty main scene is missing")
    check(not DirAccess.dir_exists_absolute(destination.path_join("src/project_creator")), "Creator source leaked into generated project")
    check(not DirAccess.dir_exists_absolute(destination.path_join("project/project_creator")), "Creator UI leaked into generated project")
    check(not DirAccess.dir_exists_absolute(destination.path_join("meow")), "Generated project must use project/ only")
    var metadata = JSON.parse_string(FileAccess.get_file_as_string(destination.path_join("meowody-project.json")))
    check(metadata.project_dir == "project" and metadata.godot_version == version and metadata.godot_cpp_commit == result.pin, "Generated version pin is incorrect")
    var config = ConfigFile.new()
    check(config.load(destination.path_join("project/project.godot")) == OK and config.get_value("application", "config/name") == request.name, "Unicode or quoted project name was not preserved")
    var settings = JSON.parse_string(FileAccess.get_file_as_string(destination.path_join("project/resources/settings.json")))
    check(settings.entries.is_empty(), "Business resources leaked into generated project")
    check(not view.generate_project(request).get("ok", true), "Nonempty destination must be protected")
    check(FileAccess.file_exists(destination.path_join("project/main.tscn")), "Protected output was altered")
    request.destination = source.path_join("nested-output")
    check(not view.generate_project(request).get("ok", true), "Nested source output must be rejected")
    view.free()
    print("[CreatorTemplate] failures=", failures)
    quit(0 if failures == 0 else 1)
