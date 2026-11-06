"""Exercise the real App, background ownership, HTTP resume and native Windows requests."""
import argparse
import ctypes
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from threading import Thread
import time
from godot_test_workspace import prepare_workspace

PAYLOAD = bytes(range(256)) * 8192


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *_args): pass
    def handle(self):
        try: super().handle()
        except (ConnectionResetError, ConnectionAbortedError): pass
    def do_GET(self):
        start = int(self.headers.get("Range", "bytes=0-").removeprefix("bytes=").split("-")[0])
        if start:
            self.server.range_requests += 1
            self.server.if_range.append(self.headers.get("If-Range"))
        self.send_response(206 if start else 200)
        self.send_header("Content-Length", str(len(PAYLOAD) - start))
        self.send_header("ETag", '"lifecycle-v1"')
        if start: self.send_header("Content-Range", f"bytes {start}-{len(PAYLOAD)-1}/{len(PAYLOAD)}")
        self.end_headers()
        try:
            for offset in range(start, len(PAYLOAD), 16384):
                self.wfile.write(PAYLOAD[offset:offset + 16384]); self.wfile.flush()
                time.sleep(0.004)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--visual", action="store_true", help="Also test native Windows minimize/restore and title-bar close")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    project = prepare_workspace()
    (project / "native_build").mkdir(); (project / "native_build/.gdignore").touch()
    env = os.environ.copy()
    env["MEOWODY_TEST_ROOT"], env["MEOWODY_TEST_OUTPUT"] = str(root), str(project)
    def command(name, argv, timeout=120, environment=None):
        with (project / (name + ".log")).open("w", encoding="utf-8") as stream:
            result = subprocess.run(argv, cwd=root, env=environment or env, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
        text = (project / (name + ".log")).read_text(encoding="utf-8", errors="replace")
        if result.returncode or "SCRIPT ERROR:" in text or "[LifecycleTest] FAIL" in text:
            raise RuntimeError(name + ":\n" + text[-16000:])
        return text
    (project / "framework").mkdir(); (project / "framework/.gdignore").touch()
    framework_env = env.copy()
    framework_env["MEOWODY_BINDING_BUILD"] = "1"
    framework_env["MEOWODY_BINDING_OUTPUT"] = str(project / "framework")
    command("framework_build", [sys.executable, "-m", "SCons"], 900, framework_env)
    command("compile", [sys.executable, "-m", "SCons", "-f", "tests/SConstruct.lifecycle"], 300)
    for name in ("profile", "temp"):
        (project / name).mkdir(); (project / name / ".gdignore").touch()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(project / "profile")
    env["TEMP"] = env["TMP"] = str(project / "temp")
    (project / "project.godot").write_text('config_version=5\n[application]\nconfig/name="Lifecycle regression"\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding="utf-8")
    (project / "lifecycle.gdextension").write_text('[configuration]\nentry_symbol="lifecycle_test_init"\ncompatibility_minimum="4.7"\n[libraries]\nwindows="res://lifecycle_test.dll"\n', encoding="utf-8")
    (project / ".godot").mkdir(); (project / ".godot/extension_list.cfg").write_text("res://lifecycle.gdextension\n", encoding="utf-8")
    (project / "resources").mkdir()
    entries = [{"address": name, "path": "res://" + name + extension, "labels": [], "group": "test"} for name, extension in (("panel", ".tscn"), ("cached", ".tscn"), ("res", ".tres"), ("spare", ".tres"))]
    (project / "resources/settings.json").write_text(json.dumps({"schema": 1, "entries": entries}), encoding="utf-8")
    for name in ("res", "spare"):
        (project / (name + ".tres")).write_text('[gd_resource type="Resource" format=3]\n[resource]\nresource_name="' + name + '"\n', encoding="utf-8")
    (project / "cached.tscn").write_text('[gd_scene format=3]\n[node name="Cached" type="Control"]\n', encoding="utf-8")
    (project / "panel.tscn").write_text('''[gd_scene load_steps=3 format=3]
[sub_resource type="Animation" id="Enter"]
resource_name = "ui_enter"
length = 60.0
[sub_resource type="AnimationLibrary" id="Library"]
_data = {&"ui_enter": SubResource("Enter")}
[node name="Panel" type="Control"]
[node name="AnimationPlayer" type="AnimationPlayer" parent="."]
libraries = {&"": SubResource("Library")}
''', encoding="utf-8")
    (project / "test.gd").write_text('''extends SceneTree
func check(value: bool, message: String) -> bool:
    if not value:
        print("[LifecycleTest] FAIL ", message)
        quit(1)
    return value
func _initialize():
    call_deferred("run_tests")
func run_tests():
    await process_frame
    var probe = ClassDB.instantiate("LifecycleProbe")
    if not check(probe != null, "probe registered"): return
    var mode := OS.get_environment("LIFECYCLE_CASE")
    if mode == "basic" or mode == "window":
        if not probe.phase(0): quit(1); return
        for i in range(5): await process_frame
        if mode == "basic":
            if not probe.phase(1): quit(1); return
            await create_timer(0.1).timeout
            if not probe.phase(2): quit(1); return
            for i in range(5): await process_frame
            if not probe.phase(3): quit(1); return
            if not probe.phase(4): quit(1); return
        else:
            root.mode = Window.MODE_MINIMIZED
            for i in range(240):
                await process_frame
                if probe.snapshot().background: break
            if not check(probe.snapshot().background, "native minimize pauses application"): return
            Engine.get_singleton("App").notification(Node.NOTIFICATION_APPLICATION_PAUSED)
            root.mode = Window.MODE_WINDOWED
            for i in range(10): await process_frame
            if not check(probe.snapshot().background and not probe.snapshot().minimized, "system pause remains after restore"): return
            Engine.get_singleton("App").notification(Node.NOTIFICATION_APPLICATION_RESUMED)
            if not check(not probe.snapshot().background, "native restore and system resume cooperate"): return
            root.title = "Lifecycle regression " + str(OS.get_process_id())
            var ready := FileAccess.open("res://.godot/window_ready.json", FileAccess.WRITE)
            ready.store_string(JSON.stringify({"pid": OS.get_process_id(), "title": root.title, "hwnd": DisplayServer.window_get_native_handle(DisplayServer.WINDOW_HANDLE, root.get_window_id())}))
            ready.close()
            for i in range(1000):
                await process_frame
                if probe.snapshot().quit_requests > 0: break
            if not check(probe.snapshot().quit_requests == 1 and not probe.snapshot().quitting, "title-bar close intercepted"): return
            probe.stop_test_audio()
            await create_timer(0.3).timeout
            if not probe.exit_case(false): quit(1); return
    elif mode == "network":
        probe.begin_network(OS.get_environment("LIFECYCLE_HTTP"))
        for i in range(1000):
            await process_frame
            if probe.network_received() > 0: break
        if not check(probe.network_received() > 0, "received bytes before suspension"): return
        if not probe.pause_network(): quit(1); return
        await create_timer(0.1).timeout
        probe.resume_network()
        for i in range(3000):
            await process_frame
            if probe.network_done(): break
        if not check(probe.network_done() and probe.check_network(), "real HTTP continuation"): return
    else:
        if not probe.exit_case(mode == "force"): quit(1); return
    probe = null
    if mode == "basic" or mode == "network":
        await create_timer(0.3).timeout
    print("[LifecycleTest] PASS ", mode)
    quit(0)
''', encoding="utf-8")
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.range_requests = 0; server.if_range = []
    worker = Thread(target=server.serve_forever, daemon=True); worker.start()
    env["LIFECYCLE_HTTP"] = f"http://127.0.0.1:{server.server_port}"
    env["LIFECYCLE_SHA"] = hashlib.sha256(PAYLOAD).hexdigest()
    try:
        for case in ("basic", "network", "confirm", "force"):
            env["LIFECYCLE_CASE"] = case
            text = command(case, [str(args.godot.resolve()), "--headless", "--audio-driver", "Dummy", "--path", str(project), "--script", "res://test.gd"], environment=env)
            if "[LifecycleTest] PASS " + case not in text: raise RuntimeError(text)
        assert server.range_requests > 0 and all(value == '"lifecycle-v1"' for value in server.if_range), server.if_range
        if args.visual:
            env["LIFECYCLE_CASE"] = "window"
            with (project / "window.log").open("w", encoding="utf-8") as stream:
                process = subprocess.Popen([str(args.godot.resolve()), "--resolution", "960x640", "--position", "-20000,-20000", "--audio-driver", "Dummy", "--path", str(project), "--script", "res://test.gd"], env=env, stdout=stream, stderr=subprocess.STDOUT)
                try:
                    ready = project / ".godot/window_ready.json"
                    deadline = time.monotonic() + 30
                    while not ready.exists() and process.poll() is None and time.monotonic() < deadline: time.sleep(0.1)
                    data = json.loads(ready.read_text(encoding="utf-8"))
                    user32 = ctypes.windll.user32
                    hwnd = int(data["hwnd"])
                    pid = ctypes.c_ulong()
                    user32.GetWindowThreadProcessId(ctypes.c_void_p(hwnd), ctypes.byref(pid))
                    if not hwnd or pid.value != data["pid"]: raise RuntimeError("Isolated test window identity mismatch")
                    user32.PostMessageW(ctypes.c_void_p(hwnd), 0x0112, 0xF060, 0) # WM_SYSCOMMAND / SC_CLOSE
                    if process.wait(timeout=30): raise RuntimeError("Native window case failed")
                finally:
                    if process.poll() is None: process.kill(); process.wait()
            text = (project / "window.log").read_text(encoding="utf-8", errors="replace")
            if "[LifecycleTest] PASS window" not in text: raise RuntimeError(text)
        reports = root / "build/lifecycle-validation"
        reports.mkdir(exist_ok=True)
        for path in project.glob("*.log"): shutil.copy2(path, reports / path.name)
        print("[LifecycleTest] PASS: ownership, reentrancy, save/quit failures, idle cleanup, HTTP Range resume" + (", native minimize/restore/close" if args.visual else ""))
    finally:
        server.shutdown(); server.server_close(); worker.join()


if __name__ == "__main__":
    main()
