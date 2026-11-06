"""Real editor exports, shared packages, version upgrades and offline resource loading."""
import argparse
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import site
import subprocess
import sys
import struct
import wave
import zlib
import zipfile
from threading import Thread
from godot_test_workspace import prepare_workspace


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--mobile-exports", action="store_true")
    parser.add_argument("--visual", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    project = prepare_workspace()
    for fixture in (project / "tests").iterdir():
        if fixture.is_dir() and fixture.name != "meowody_resources": (fixture / ".gdignore").touch()
    (project / "native_build").mkdir()
    (project / "native_build/.gdignore").touch()
    env = os.environ.copy()
    env["PYTHONPATH"] = os.pathsep.join(site.getsitepackages() + [site.getusersitepackages()])
    for name in ("profile", "temp"):
        (project / name).mkdir()
        (project / name / ".gdignore").touch()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(project / "profile")
    env["TEMP"] = env["TMP"] = str(project / "temp")
    env["MEOWODY_TEST_ROOT"], env["MEOWODY_TEST_OUTPUT"] = str(root), str(project)
    def run(name, command, timeout=60):
        with (project / (name + ".log")).open("w", encoding="utf-8") as stream:
            result = subprocess.run(command, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
        output = (project / (name + ".log")).read_text(encoding="utf-8", errors="replace")
        if result.returncode or "SCRIPT ERROR:" in output:
            raise RuntimeError(name + ":\n" + output[-14000:])
        return output
    run("compile", [sys.executable, "-m", "SCons", "-f", "tests/SConstruct.resources"], 600)
    shutil.copytree(root / "meow/addons/meowody_resources", project / "addons/meowody_resources")
    (project / "resources_test.gdextension").write_text('[configuration]\nentry_symbol="resources_test_init"\ncompatibility_minimum="4.7"\n[libraries]\nwindows="res://resources_test.dll"\n', encoding="utf-8")
    (project / ".godot").mkdir(exist_ok=True)
    (project / ".godot/extension_list.cfg").write_text("res://resources_test.gdextension\n", encoding="utf-8")
    (project / "project.godot").write_text('config_version=5\n[application]\nconfig/name="Resources test"\nrun/main_scene="res://boot.tscn"\n[editor_plugins]\nenabled=PackedStringArray("res://addons/meowody_resources/plugin.cfg")\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding="utf-8")
    (project / "boot.tscn").write_text('[gd_scene format=3]\n[node name="Boot" type="Node"]\n', encoding="utf-8")
    (project / "item.tres").write_text('[gd_resource type="Resource" format=3]\n[resource]\nresource_name="version1"\n', encoding="utf-8")
    (project / "shared.tres").write_text('[gd_resource type="Resource" format=3]\n[resource]\nresource_name="shared"\n', encoding="utf-8")
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress((b"\0" + b"\xff\x40\x20\xff" * 4) * 4)) + chunk(b"IEND", b"")
    (project / "texture.png").write_bytes(png)
    for name in ("scene", "scene_two"):
        (project / (name + ".tscn")).write_text('[gd_scene load_steps=2 format=3]\n[ext_resource type="Texture2D" path="res://texture.png" id="1"]\n[node name="Sprite" type="Sprite2D"]\ntexture=ExtResource("1")\n', encoding="utf-8")
    with wave.open(str(project / "sound.wav"), "wb") as audio:
        audio.setnchannels(1); audio.setsampwidth(2); audio.setframerate(22050); audio.writeframes(b"\0\0" * 2205)
    for name in ("one", "two"):
        (project / f"{name}.tres").write_text('[gd_resource type="Resource" load_steps=2 format=3]\n[ext_resource type="Resource" path="res://shared.tres" id="1"]\n[resource]\nmetadata/shared=ExtResource("1")\n', encoding="utf-8")
    server_root = project.parent / "published"
    server_root.mkdir()
    class Handler(SimpleHTTPRequestHandler):
        protocol_version = "HTTP/1.1"
        def log_message(self, message, *args):
            with (project / "http.log").open("a", encoding="utf-8") as stream: stream.write(message % args + "\n")
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(Handler, directory=str(server_root)))
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    settings = {"schema": 1, "compatibility": "test1", "active_profile": "development", "play_mode": "source",
                "groups": [{"name": name, "remote": True, "startup": True} for name in ("A", "B")],
                "entries": [{"address": "item", "path": "res://item.tres", "group": "A", "labels": ["startup"]},
                            {"address": "scene", "path": "res://scene.tscn", "group": "A", "labels": []},
                            {"address": "scene_two", "path": "res://scene_two.tscn", "group": "B", "labels": []},
                            {"address": "sound", "path": "res://sound.wav", "group": "A", "labels": []},
                            {"address": "one", "path": "res://one.tres", "group": "A", "labels": []},
                            {"address": "two", "path": "res://two.tres", "group": "B", "labels": []}],
                "profiles": {"development": {"output": str(server_root), "remote_root": f"http://127.0.0.1:{server.server_port}", "presets": {"windows": "Windows Desktop", "android": "Android", "ios": "iOS"}}}}
    write(project / "resources/settings.json", settings)
    build_command = [sys.executable, str(project / "addons/meowody_resources/build.py"), "--project", str(project), "--godot", str(args.godot.resolve())]
    (project / "runtime_test.gd").write_text('''extends SceneTree
func _initialize():
    call_deferred("run_test")
func run_test():
    var probe = ClassDB.instantiate("MeowodyResourceProbe")
    probe.begin(OS.get_environment("SOURCE") == "1")
    print("[ResourcesTest] initialization started")
    if OS.get_environment("CANCEL_INIT") == "1":
        probe.cancel_initialization()
        probe.tick()
        var cancelled: bool = probe.initialized() and not probe.ready()
        probe = null
        quit(0 if cancelled else 1)
        return
    for i in range(15000):
        probe.tick()
        if probe.initialized(): break
        await create_timer(0.001).timeout
    if not probe.ready():
        push_error(probe.message())
        quit(1)
        return
    if OS.get_environment("FALLBACK") == "1" and not probe.fallback():
        push_error("Expected offline fallback")
        quit(1)
        return
    probe.start_load()
    for i in range(15000):
        probe.tick()
        if probe.loaded(): break
        await create_timer(0.001).timeout
    var ok: bool = probe.loaded() and probe.verify(OS.get_environment("EXPECTED"))
    probe = null
    print("[ResourcesTest] PASS" if ok else "[ResourcesTest] FAIL")
    quit(0 if ok else 1)
''', encoding="utf-8")
    runtime = [str(args.godot.resolve()), "--headless", "--path", str(project), "--script", "res://runtime_test.gd"]
    try:
        run("build_v1", build_command, 900)
        display = ["--resolution", "1920x1080", "--position", "-20000,-20000"] if args.visual else ["--headless"]
        run("editor", [str(args.godot.resolve()), *display, "--editor", "--path", str(project), "--", "--resource-editor-test"])
        published = server_root / "windows"
        analysis = json.loads((published / "analysis.json").read_text(encoding="utf-8"))
        assert sum("res://shared.tres" in pack["files"] for pack in analysis["packs"].values()) == 1
        builtin = json.loads((project / "resources/catalog.json").read_text(encoding="utf-8"))
        (project / "export_presets.cfg").write_text('[preset.0]\nname="App"\nplatform="Windows Desktop"\nexport_filter="all_resources"\ninclude_filter=""\nexclude_filter=""\n[preset.0.options]\ntexture_format/bptc=true\n', encoding="utf-8")
        application = server_root / "application.zip"
        run("app_export", [str(args.godot.resolve()), "--headless", "--path", str(project), "--export-pack", "App", str(application)])
        with zipfile.ZipFile(application) as archive:
            names = archive.namelist()
            assert "resources/catalog.json" in names, names
            assert not any(name.endswith("-item.res") or name.endswith("-shared.res") or name.endswith("-scene.scn") or name.endswith(".ctex") for name in names), names
        write(project / "test_catalog.json", builtin)
        env.update(SOURCE="1", EXPECTED="version1", FALLBACK="0")
        run("source", runtime)
        env["SOURCE"] = "0"
        env["CANCEL_INIT"] = "1"
        run("cancel_initialization", runtime)
        assert not list((project / "profile").rglob("test_resources/*/catalog.json"))
        env["CANCEL_INIT"] = "0"
        run("remote_v1", runtime)
        run("rebuild_unchanged", build_command, 900)
        report = json.loads((published / "report.json").read_text(encoding="utf-8"))
        assert not report["rebuilt"] and report["reused"], report
        (project / "item.tres").write_text('[gd_resource type="Resource" format=3]\n[resource]\nresource_name="version2"\n', encoding="utf-8")
        run("build_v2", build_command, 900)
        report = json.loads((published / "report.json").read_text(encoding="utf-8"))
        assert len(report["rebuilt"]) == 1 and report["reused"], report
        if args.mobile_exports:
            for target in ("android", "ios"):
                run("build_" + target, build_command + ["--platform", target], 900)
                target_catalog = json.loads((project / "resources/catalog.json").read_text(encoding="utf-8"))
                assert target_catalog["platform"] == target and target_catalog["packs"]
        # Remove the source resource so the actual downloaded PCK must supply it.
        (project / "item.tres").unlink()
        for name in ("texture.png", "texture.png.import", "sound.wav", "sound.wav.import", "scene.tscn"):
            (project / name).unlink(missing_ok=True)
        imported = project / ".godot/imported"
        if imported.is_dir(): shutil.rmtree(imported)
        env["EXPECTED"] = "version2"
        run("remote_upgrade", runtime)
        pointer = published / "latest.json"
        current = json.loads(pointer.read_text(encoding="utf-8"))
        candidate = json.loads((published / current["catalog"]).read_text(encoding="utf-8"))
        candidate["version"] = "invalid-content"
        victim = next(key for key, pack in analysis["packs"].items() if "res://item.tres" in pack["files"])
        pack = candidate["packs"][victim]
        shutil.copyfile(published / pack["file"], published / ("packs/" + "1" * 64 + ".pck"))
        pack.update(file="packs/" + "1" * 64 + ".pck", sha256="1" * 64, local="")
        corrupt_catalog = published / "versions/invalid-content/catalog.json"
        write(corrupt_catalog, candidate)
        write(pointer, {"catalog": "versions/invalid-content/catalog.json", "sha256": hashlib.sha256(corrupt_catalog.read_bytes()).hexdigest()})
        env["FALLBACK"] = "1"
        run("checksum_failure_fallback", runtime)
        for cached in (project / "profile").rglob("test_resources/*/catalog.json"):
            assert json.loads(cached.read_text(encoding="utf-8"))["version"] != "invalid-content"
        pointer.write_text('{"catalog":"../escape","sha256":"invalid"}', encoding="utf-8")
        env["FALLBACK"] = "1"
        run("invalid_pointer_fallback", runtime)
        server.shutdown()
        server.server_close()
        thread.join()
        run("offline_fallback", runtime)
        print("[ResourcesTest] PASS: source, editor, shared dependencies, native PCK, application export, unchanged reuse, incremental upgrade, initialization cancellation, checksum rollback, corrupt pointer and offline fallback")
    finally:
        if thread.is_alive():
            server.shutdown(); server.server_close(); thread.join()


if __name__ == "__main__":
    main()
