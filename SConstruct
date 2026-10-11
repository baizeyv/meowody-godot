import os
import sys
import json
from pathlib import Path
from SCons.Variables import BoolVariable

def scons_path(path):
    return Path(path).as_posix()


project_config_path = Path("meowody-project.json")
project_config = json.loads(project_config_path.read_text(encoding="utf-8")) if project_config_path.exists() else {}
project_dir = project_config.get("project_dir", "meow")
api_version = ARGUMENTS.get("api_version", project_config.get("api_version", "4.7"))
if "api_version" not in ARGUMENTS and "custom_api_file" not in ARGUMENTS and project_config.get("custom_api_file"):
    ARGUMENTS["custom_api_file"] = project_config["custom_api_file"]
ARGUMENTS.setdefault(
    "use_hot_reload",
    "no" if ARGUMENTS.get("target", "template_debug") == "template_release" else "yes",
)
env = SConscript(
    "godot-cpp/SConstruct",
    exports={"api_version": api_version},
)

# 引入 godot-cpp 的构建逻辑后：
env["CXXFLAGS"] = [flag for flag in env["CXXFLAGS"] if str(flag) not in ("/std:c++17", "-std=c++17")]
if env["platform"] == "windows":
    # MSVC 编译器设置 C++20
    env.Append(CXXFLAGS=["/std:c++20"])
else:
    # GCC / Clang 编译器设置 C++20
    env.Append(CXXFLAGS=["-std=c++20"])
env.Append(CPPPATH=["src/"])

is_release = env.get("target", "template_debug") in ["template_release", "release"]

opts = Variables()
opts.Add(BoolVariable(
    "durable_saves",
    "Use durable native file replacement",
    is_release,
))
opts.Add(BoolVariable(
    "log_records",
    "Capture and publish log records",
    not is_release,
))
opts.Update(env)

def option_enabled(value):
    if isinstance(value, str):
        return value.lower() in ("1", "yes", "true", "on")
    return bool(value)


durable_saves = option_enabled(env["durable_saves"])
log_records = option_enabled(env["log_records"])

def select_sources():
    candidates = [
        os.path.join(root, filename)
        for root, _, filenames in os.walk("src")
        for filename in filenames
        if filename.endswith(".cpp")
    ]

    selected = []
    for source in candidates:
        normalized = source.replace("\\", "/")

        if "/platform/" in normalized:
            continue
        if normalized.startswith("src/editor/") and is_release:
            continue
        if normalized == "src/editor_registration_stub.cpp" and not is_release:
            continue
        if normalized == "src/runtime/debug/debug_disabled.cpp" and not is_release:
            continue
        if normalized == "src/log/log_records_stub.cpp":
            continue
        if normalized == "src/log/log_stack.cpp":
            if not is_release and env["platform"] == "windows":
                continue
        elif normalized == "src/log/log_records.cpp":
            if not log_records:
                continue
        elif normalized == "src/runtime/save/atomic_file.cpp":
            if durable_saves:
                continue

        selected.append(source)

    if not is_release and env["platform"] == "windows":
        selected.append("src/log/platform/log_stack_windows.cpp")
    if not log_records:
        selected.append("src/log/log_records_stub.cpp")
    if durable_saves:
        selected.append("src/runtime/save/platform/atomic_file_native.cpp")

    return sorted(selected)


sources = select_sources()

handwritten_sources = list(sources)
generated_outputs = SConscript("reflect_codegen/SConscript", exports={"env": env})
sources += [output for output in generated_outputs if str(output).endswith(".cpp")]
env.Depends(handwritten_sources, generated_outputs)

if is_release:
    if env.get("disable_exceptions", True):
        if "msvc" in env["TOOLS"]:
            env.Append(CXXFLAGS=["/GR-", "/EHsc-"])
        else:
            env.Append(CXXFLAGS=["-fno-exceptions", "-fno-rtti"])

    if "msvc" not in env["TOOLS"]:
        env.Append(CCFLAGS=["-ffunction-sections", "-fdata-sections", "-fvisibility=hidden"])
        env.Append(CXXFLAGS=["-fvisibility-inlines-hidden"])
        
        # Keep godot-cpp's selected optimize=speed/speed_trace/size flags.

        if env["platform"] in ["linux", "android"]:
            env.Append(LINKFLAGS=["-Wl,--gc-sections", "-Wl,-s"])
        elif env["platform"] in ["macos", "ios"]:
            env.Append(LINKFLAGS=["-Wl,-dead_strip"])
    else:
        env.Append(CCFLAGS=["/Gy", "/Gw"])
        env.Append(LINKFLAGS=["/OPT:REF", "/OPT:ICF"])
elif "msvc" in env["TOOLS"]:
    env.Append(CXXFLAGS=["/EHsc", "/Z7"])
    env.Append(LINKFLAGS=["/DEBUG:FULL", "/INCREMENTAL:NO"])

if not is_release and env["platform"] == "windows":
    env.Append(LIBS=["dbghelp"])

# The editor publishes this library only after SCons has exited successfully.
output_dir = os.environ.get("MEOWODY_BINDING_OUTPUT", project_dir + "/.godot/binding-build") if os.environ.get("MEOWODY_BINDING_BUILD") == "1" else project_dir + "/bin"
target_path = output_dir + "/libmeowody.%s.%s.%s" % (
    env["platform"],
    env["target"],
    env["arch"],
)
if env["platform"] == "windows":
    target_path += ".dll"
    if not is_release and "msvc" in env["TOOLS"]:
        env["PDB"] = target_path.removesuffix(".dll") + ".pdb"

if env["platform"] == "ios":
    library = env.StaticLibrary(target_path, source=sources)
else:
    library = env.SharedLibrary(target_path, source=sources)

Default(library)
