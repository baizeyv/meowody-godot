"""Build helper for the native Godot project creator; no desktop UI."""
import json
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile

FRAMEWORK_URL = "https://github.com/baizeyv/meowody-godot.git"
GODOT_BUILDS = "https://github.com/godotengine/godot-builds"
CREATE_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)


def run(command, cwd, log=print, env=None):
    # Never log command arguments: Git credentials can be embedded in URLs.
    environment = dict(os.environ if env is None else env)
    environment["GIT_TERMINAL_PROMPT"] = "0"
    with subprocess.Popen([str(part) for part in command], cwd=cwd, env=environment, stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          encoding="utf-8", errors="replace", creationflags=CREATE_NO_WINDOW) as process:
        output = []
        for line in process.stdout:
            output.append(line)
            log(line.rstrip())
        if process.wait():
            raise RuntimeError(f"{Path(str(command[0])).name} failed; see the log.")
    return "".join(output).strip()


def release_versions():
    base = "https://api.github.com/repos/godotengine/godot-builds/releases"
    def fetch(url):
        request = urllib.request.Request(url, headers={"User-Agent": "MeowodyProjectCreator", "Accept": "application/vnd.github+json"})
        with urllib.request.urlopen(request, timeout=20) as response:
            return json.load(response)
    latest = fetch(base + "/latest")
    releases = fetch(base + "?per_page=100")
    versions = {release["tag_name"] for release in [latest, *releases]
                if not release.get("prerelease") and not release.get("draft")
                and re.fullmatch(r"4\.\d+(?:\.\d+)?-stable", release.get("tag_name", ""))}
    if not versions:
        raise ValueError("No official stable Godot 4 release is available.")
    return sorted(versions, key=lambda version: tuple(int(part) for part in version.removesuffix("-stable").split(".")), reverse=True)


def download_engine(version, cache, log=print):
    if not re.fullmatch(r"4\.\d+(?:\.\d+)?-stable", version):
        raise ValueError("Use an official stable version, for example 4.7.2-stable.")
    directory = Path(cache) / "engines" / version
    ready = directory / "ready.json"
    if ready.exists():
        executable = directory / json.loads(ready.read_text(encoding="utf-8"))["executable"]
        if executable.is_file():
            return executable
    filename = f"Godot_v{version}_win64.exe.zip"
    release_url = GODOT_BUILDS + "/releases/download/" + version + "/"
    directory.mkdir(parents=True, exist_ok=True)
    archive = directory / "download.zip.part"
    log("Downloading Godot " + version)
    request = urllib.request.Request(release_url + filename, headers={"User-Agent": "MeowodyProjectCreator"})
    with urllib.request.urlopen(request, timeout=60) as response, archive.open("wb") as output:
        shutil.copyfileobj(response, output)
    request = urllib.request.Request(release_url + "SHA512-SUMS.txt", headers={"User-Agent": "MeowodyProjectCreator"})
    with urllib.request.urlopen(request, timeout=60) as response:
        checksums = response.read().decode("utf-8")
    expected = next((line.split()[0] for line in checksums.splitlines()
                     if len(line.split()) == 2 and line.split()[1].lstrip("*") == filename), None)
    with archive.open("rb") as downloaded:
        actual = hashlib.file_digest(downloaded, "sha512").hexdigest()
    if expected is None or actual != expected:
        raise RuntimeError("Engine checksum validation failed; run creation again.")
    with zipfile.ZipFile(archive) as package:
        members = package.infolist()
        for member in members:
            if not (directory / member.filename).resolve().is_relative_to(directory.resolve()):
                raise ValueError("Engine archive contains a path outside its destination.")
        package.extractall(directory)
    archive.unlink()
    executables = [path for path in directory.rglob("*.exe") if not path.stem.endswith("_console")]
    if len(executables) != 1:
        raise ValueError("Cannot find the downloaded Godot editor.")
    executable = executables[0]
    ready.write_text(json.dumps({"executable": executable.relative_to(directory).as_posix()}), encoding="utf-8")
    return executable


def parse_engine_version(version):
    match = re.fullmatch(r"(4)\.(\d+)\.(\d+)\.stable\.official\.[0-9a-f]+", version)
    if not match:
        raise ValueError("First version supports official stable Godot 4 Windows x64 builds.")
    return version, f"4.{match[2]}"


def inspect_engine(executable):
    executable = Path(executable).resolve()
    return parse_engine_version(run([executable, "--version"], executable.parent, log=lambda _: None))


def local_engine(executable):
    """Accept Windows x64 PE files reporting an official stable Godot 4 version."""
    executable = Path(executable).resolve()
    with executable.open("rb") as file:
        header = file.read(64)
        if len(header) != 64 or header[:2] != b"MZ":
            raise ValueError("Not a Windows executable.")
        file.seek(int.from_bytes(header[60:64], "little"))
        if file.read(6) != b"PE\0\0\x64\x86":
            raise ValueError("Use a Windows x64 Godot editor.")
    result = subprocess.run([str(executable), "--version"], cwd=executable.parent,
                            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            encoding="utf-8", errors="replace", creationflags=CREATE_NO_WINDOW, timeout=5)
    if result.returncode:
        raise ValueError("Godot version check failed.")
    version, api = parse_engine_version(result.stdout.strip())
    help_result = subprocess.run([str(executable), "--help"], cwd=executable.parent,
                                 stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 encoding="utf-8", errors="replace", creationflags=CREATE_NO_WINDOW, timeout=5)
    if help_result.returncode or "--dump-extension-api" not in help_result.stdout or "--editor" not in help_result.stdout:
        raise ValueError("Choose a Godot editor, not an export template.")
    return {"path": str(executable), "version": version, "api": api}


def engine_search_roots(cache):
    roots = [Path.home() / name for name in ("Downloads", "Desktop", "scoop/apps/godot")]
    roots.append(Path(cache) / "engines")
    for key in ("ProgramFiles", "ProgramFiles(x86)"):
        if os.environ.get(key):
            roots.append(Path(os.environ[key]))
    if os.environ.get("LOCALAPPDATA"):
        roots.append(Path(os.environ["LOCALAPPDATA"]) / "Programs")
    # Portable editors are commonly unpacked beneath a drive's tools/software folder.
    for drive in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
        root = Path(drive + ":/")
        if not root.is_dir():
            continue
        try:
            roots.extend(path for path in root.iterdir() if path.is_dir() and any(
                word in path.name.casefold() for word in ("godot", "tool", "develop", "engine", "program", "software", "app", "工具", "软件", "开发")))
        except OSError:
            pass
    return roots


def discover_engines(cache, seeds=(), roots=None, log=print):
    candidates = list(seeds)
    for directory in os.get_exec_path():
        try:
            candidates.extend(Path(directory).glob("*godot*.exe"))
        except OSError:
            pass
    pending = [(Path(root), 0) for root in (engine_search_roots(cache) if roots is None else roots)]
    visited = set()
    while pending:
        directory, depth = pending.pop()
        try:
            key = str(directory.resolve()).casefold()
            if key in visited or getattr(directory.stat(follow_symlinks=False), "st_file_attributes", 0) & 0x400:
                continue
            visited.add(key)
            with os.scandir(directory) as entries:
                for entry in entries:
                    if entry.is_file(follow_symlinks=False) and "godot" in entry.name.casefold() and entry.name.lower().endswith(".exe"):
                        candidates.append(entry.path)
                    elif depth < 3 and entry.is_dir(follow_symlinks=False) and entry.name.lower() not in (
                            ".git", ".godot", "node_modules", "__pycache__", "windows", "cache", "temp"):
                        pending.append((Path(entry.path), depth + 1))
        except OSError:
            continue
    found, seen = [], set()
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate)
        key = str(path.resolve()).casefold()
        if key in seen or path.stem.lower().endswith("_console"):
            continue
        seen.add(key)
        try:
            item = local_engine(path)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            continue
        found.append(item)
        log("Found Godot " + item["version"] + " — " + item["path"])
    return sorted(found, key=lambda item: (tuple(int(part) for part in item["version"].split(".")[:3]), item["path"].casefold()), reverse=True)


def preflight():
    if os.name != "nt":
        raise RuntimeError("Project creation currently supports Windows x64.")
    for executable, instruction in (("git", "Install Git for Windows."), ("python", "Install Python and add it to PATH."),
                                    ("scons", "Run: python -m pip install scons")):
        if not shutil.which(executable):
            raise RuntimeError(instruction)
    try:
        run(["python", "-c", "import os, shutil; from pathlib import Path; from clang.cindex import Config, Index; p = os.environ.get('LIBCLANG_PATH'); c = shutil.which('clang'); p = p or (str(Path(c).resolve().parent) if c else None); Config.set_library_path(p) if p else None; Index.create()"],
            Path.cwd(), log=lambda _: None)
    except RuntimeError as error:
        raise RuntimeError("Install matching clang Python bindings and LLVM libclang; set LIBCLANG_PATH if needed.") from error
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not shutil.which("cl") and (not vswhere.is_file() or not run(
            [vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
            vswhere.parent, log=lambda _: None)):
        raise RuntimeError("Install Visual Studio C++ build tools and a Windows SDK.")


def checkout(url, ref, destination, log):
    if not ref or ref.startswith("-"):
        raise ValueError("Enter a branch, tag or commit.")
    run(["git", "init", destination], destination.parent, log)
    run(["git", "remote", "add", "origin", url], destination, log)
    run(["git", "fetch", "--depth", "1", "origin", ref], destination, log)
    run(["git", "checkout", "--detach", "FETCH_HEAD"], destination, log)
    return run(["git", "rev-parse", "HEAD"], destination, log=lambda _: None)


def copy_source(source, destination):
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns(
        ".git", ".godot", "__pycache__", "node_modules", "dist", "*.pyc", "*.obj", "*.o", "*.os",
        "*.dll", "*.lib", "*.exp", "*.pdb", "*.so", "*.a", "*.dylib", "*.exe", "*.bak"))


def generate_project(source, destination, name, engine_version, api_version, framework_commit):
    """Copy only framework sources; destination must be new or empty."""
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if not name.strip() or any(ord(character) < 32 for character in name):
        raise ValueError("Project name must be non-empty and contain no control characters.")
    if destination == source or destination.is_relative_to(source):
        raise ValueError("Create the new project outside its downloaded framework checkout.")
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
        raise ValueError("Destination must be a new or empty directory; nothing will be overwritten.")
    dependency = json.loads((source / "framework-dependencies.json").read_text(encoding="utf-8"))["godot_cpp"]
    if dependency["url"] != "https://github.com/godotengine/godot-cpp.git" or not re.fullmatch(r"[0-9a-f]{40}", dependency["commit"]):
        raise ValueError("Framework must pin an official godot-cpp commit.")
    destination.mkdir(parents=True, exist_ok=True)
    for directory in ("runtime", "editor", "log", "reflection"):
        copy_source(source / "src" / directory, destination / "src" / directory)
    for filename in ("register_types.cpp", "register_types.h", "editor_registration.h", "editor_registration_stub.cpp"):
        shutil.copy2(source / "src" / filename, destination / "src" / filename)
    (destination / "src/ui/generated").mkdir(parents=True)
    for directory in ("reflect_codegen", "tools/godot_mcp", ".agents/skills/meowody-framework"):
        copy_source(source / directory, destination / directory)
    for filename in ("SConstruct", ".gitignore", ".clang-format", "framework-dependencies.json"):
        shutil.copy2(source / filename, destination / filename)
    (destination / "FRAMEWORK.md").write_text((source / "README.md").read_text(encoding="utf-8").replace("meow/", "project/").replace("--path meow", "--path project"), encoding="utf-8")
    skill = destination / ".agents/skills/meowody-framework/SKILL.md"
    if skill.exists():
        skill.write_text(skill.read_text(encoding="utf-8").replace("meow/", "project/").replace("Root README.md is the single handbook", "FRAMEWORK.md is the framework handbook"), encoding="utf-8")
    project = destination / "project"
    copy_source(source / "meow/addons", project / "addons")
    resource_settings = source / "meow/resources/settings.json"
    if resource_settings.is_file():
        (project / "resources").mkdir()
        shutil.copy2(resource_settings, project / "resources/settings.json")
    (project / "bin").mkdir()
    extension = (source / "meow/bin/meowody.gdextension").read_text(encoding="utf-8")
    extension = re.sub(r'compatibility_minimum\s*=\s*"[^"]+"', f'compatibility_minimum = "{api_version}"', extension)
    (project / "bin/meowody.gdextension").write_text(extension, encoding="utf-8")
    plugins = [f"res://addons/{path.parent.name}/plugin.cfg" for path in sorted((project / "addons").glob("*/plugin.cfg"))]
    quoted_plugins = ", ".join(json.dumps(plugin) for plugin in plugins)
    (project / "project.godot").write_text(f'''config_version=5
[application]
config/name={json.dumps(name, ensure_ascii=False)}
run/main_scene="res://main.tscn"
config/features=PackedStringArray("{api_version}", "GL Compatibility")
[editor_plugins]
enabled=PackedStringArray({quoted_plugins})
[rendering]
renderer/rendering_method="gl_compatibility"
textures/vram_compression/import_etc2_astc=true
''', encoding="utf-8")
    (project / "main.tscn").write_text('[gd_scene format=3]\n\n[node name="Main" type="Node"]\n', encoding="utf-8")
    config = {"project_dir": "project", "api_version": api_version, "custom_api_file": "godot-api/extension_api.json",
              "godot_version": engine_version, "framework_url": FRAMEWORK_URL, "framework_commit": framework_commit,
              "godot_cpp_commit": dependency["commit"]}
    (destination / "meowody-project.json").write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    (destination / "README.md").write_text(f'''# {name}

Godot project: `project/project.godot`. Framework reference: FRAMEWORK.md.
Build Debug with `scons`; build Release with `scons target=template_release`.
Both plain builds and editor reloads read meowody-project.json and godot-api/extension_api.json.
Install Git, Python, SCons, matching clang Python bindings/libclang, MSVC and a Windows SDK.
MCP server: `cd tools/godot_mcp`, `npm ci --ignore-scripts`, `npm run build` (Node.js 20+).
Start MCP with `node tools/godot_mcp/dist/index.js --project "ABSOLUTE_PATH/project"`.
This project has its own Git repository and a pinned godot-cpp submodule; no commits are created automatically.
After cloning this project, restore dependencies with `git submodule update --init --recursive`.
Commit .gitmodules, the godot-cpp gitlink and framework-dependencies.json together to preserve the dependency version.
The handbook's upstream regression commands require the original framework repository; test projects are not included here.
''', encoding="utf-8")
    return dependency


def prepare_dependency(source, destination, dependency, log):
    gitlink = run(["git", "ls-files", "--stage", "godot-cpp"], source, log=lambda _: None)
    if gitlink.startswith("160000 "):
        if gitlink.split()[1] != dependency["commit"]:
            raise ValueError("Submodule commit differs from framework-dependencies.json.")
    run(["git", "init", destination], destination.parent, log)
    checkout(dependency["url"], dependency["commit"], destination / "godot-cpp", log)
    # --force registers the dependency even when the template ignores its checkout.
    run(["git", "submodule", "add", "--force", dependency["url"], "godot-cpp"], destination, log)
    run(["git", "submodule", "absorbgitdirs", "godot-cpp"], destination, log)
    run(["git", "submodule", "update", "--init", "--recursive"], destination, log)


def build_and_verify(destination, godot, log):
    destination = Path(destination)
    environment = os.environ.copy()
    for folder, keys in (("profile", ("APPDATA", "LOCALAPPDATA")), ("temp", ("TEMP", "TMP"))):
        directory = destination / "build/creator" / folder
        directory.mkdir(parents=True, exist_ok=True)
        (directory / ".gdignore").touch()
        environment.update({key: str(directory.resolve()) for key in keys})
    api_directory = destination / "godot-api"
    api_directory.mkdir(exist_ok=True)
    run([godot, "--headless", "--dump-extension-api"], api_directory, log, environment)
    api = json.loads((api_directory / "extension_api.json").read_text(encoding="utf-8"))
    if api["header"].get("precision") != "single":
        raise ValueError("Double-precision engine builds are not supported.")
    log("Building Debug extension; this can take several minutes.")
    run(["scons", "platform=windows", "arch=x86_64", "target=template_debug"], destination, log)
    project = destination / "project"
    # Avoid touching the user's editor profile, import cache or temporary files.
    output = run([godot, "--headless", "--editor", "--path", project, "--import"], destination, log, environment)
    if "SCRIPT ERROR" in output or "ERROR:" in output:
        raise RuntimeError("Godot import reported errors; see the log.")
    probe = project / "creator_verify.gd"
    probe.write_text('''extends SceneTree
func _initialize():
    for native_class in ["SpriteAtlas", "MultiPatchRect", "VirtualList", "ArcProgress"]:
        if not ClassDB.class_exists(native_class):
            push_error("Missing framework class: " + native_class)
            quit(1)
            return
    print("[Creator] framework loaded")
    quit(0)
''', encoding="utf-8")
    try:
        output = run([godot, "--headless", "--path", project, "--script", "res://creator_verify.gd"], destination, log, environment)
        if "[Creator] framework loaded" not in output or "ERROR:" in output:
            raise RuntimeError("Framework runtime validation failed.")
        output = run([godot, "--headless", "--path", project, "--quit-after", "3"], destination, log, environment)
        if "ERROR:" in output:
            raise RuntimeError("Empty scene validation failed.")
    finally:
        probe.unlink(missing_ok=True)
        probe.with_suffix(".gd.uid").unlink(missing_ok=True)


def create_project(name, destination, ref, godot, cache, log=print):
    destination = Path(destination).resolve()
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
        raise ValueError("Destination must be a new or empty directory.")
    if not name.strip():
        raise ValueError("Enter a project name.")
    if not ref or ref.startswith("-"):
        raise ValueError("Enter a framework branch, tag or commit.")
    preflight()
    Path(cache).mkdir(parents=True, exist_ok=True)
    godot = Path(godot) if isinstance(godot, Path) else download_engine(godot, cache, log)
    version, api_version = inspect_engine(godot)
    if api_version != "4.7":
        log("Godot " + api_version + " is an experimental target; compile/load validation is required.")
    # Only the downloaded source is temporary; failed output and logs remain reviewable.
    with tempfile.TemporaryDirectory(prefix="framework-", dir=cache) as temporary:
        source = Path(temporary) / "source"
        log("Fetching framework " + ref)
        commit = checkout(FRAMEWORK_URL, ref, source, log)
        if not (source / "framework-dependencies.json").is_file():
            raise ValueError("Selected framework revision lacks framework-dependencies.json. Publish the creator changes first.")
        dependency = generate_project(source, destination, name, version, api_version, commit)
        log_path = destination / "build/creator/create.log"
        log_path.parent.mkdir(parents=True)
        with log_path.open("w", encoding="utf-8") as file:
            def report(message):
                file.write(message + "\n")
                file.flush()
                log(message)
            report("Framework commit: " + commit)
            report("Godot version: " + version)
            try:
                prepare_dependency(source, destination, dependency, report)
                build_and_verify(destination, godot.resolve(), report)
            except Exception as error:
                report("FAILED: " + str(error))
                raise
            report("Project creation and validation completed.")
    return godot.resolve()


def rebuild_project(destination, engine, log=print):
    destination, engine = Path(destination).resolve(), Path(engine).resolve()
    preflight()
    version, _ = inspect_engine(engine)
    config = json.loads((destination / "meowody-project.json").read_text(encoding="utf-8"))
    if config["godot_version"] != version:
        raise ValueError("Use the Godot version recorded when this project was created.")
    if not (destination / "godot-cpp/SConstruct").is_file():
        raise ValueError("Dependency download is incomplete; create in a new empty directory.")
    gitlink = run(["git", "ls-files", "--stage", "godot-cpp"], destination, log=lambda _: None)
    if not gitlink.startswith("160000 " + config["godot_cpp_commit"] + " ") or not (destination / ".gitmodules").is_file():
        raise ValueError("The pinned godot-cpp submodule is not registered correctly.")
    log_path = destination / "build/creator/rebuild.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as file:
        def report(line):
            file.write(line + "\n")
            file.flush()
            log(line)
        build_and_verify(destination, engine, report)
    return engine


def handle_request(request):
    operation = request["operation"]
    if operation == "discover":
        return {"engines": discover_engines(request["cache"], (request.get("engine", ""), request.get("current_engine", "")))}
    if operation == "inspect":
        return {"engines": [local_engine(request["engine"])]}
    if operation == "versions":
        return {"versions": release_versions()}
    if operation == "preflight":
        preflight()
        return {"message": "Toolchain ready."}
    if operation == "create":
        engine = Path(request["engine"]) if request["local"] else request["version"]
        executable = create_project(request["name"], request["destination"], request["ref"], engine, request["cache"])
    elif operation == "rebuild":
        executable = rebuild_project(request["destination"], request["engine"])
    else:
        raise ValueError("Unknown creator operation.")
    return {"engine": str(executable), "destination": str(Path(request["destination"]).resolve())}


if __name__ == "__main__":
    import sys
    # Requests are files, never shell expressions; stdout is the live build log.
    response_path = Path(sys.argv[2])
    try:
        response = handle_request(json.loads(Path(sys.argv[1]).read_text(encoding="utf-8")))
        response["ok"] = True
    except Exception as error:
        response = {"ok": False, "message": str(error)}
        print("FAILED: " + str(error), flush=True)
    response_path.write_text(json.dumps(response, ensure_ascii=False), encoding="utf-8")
    sys.exit(0 if response["ok"] else 1)
