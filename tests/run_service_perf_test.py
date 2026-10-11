"""Isolated native service regression tests and steady-frame benchmarks."""
import argparse
import shutil
from godot_test_workspace import prepare_workspace
import os
from pathlib import Path
import subprocess
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread


PAYLOAD = bytes(range(256)) * 8192


class DownloadHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *_args):
        pass

    def handle(self):
        try:
            super().handle()
        except (ConnectionResetError, ConnectionAbortedError):
            pass  # Pause closes a persistent connection while the server waits.

    def do_GET(self):
        start = int(self.headers.get("Range", "bytes=0-").removeprefix("bytes=").split("-")[0])
        self.send_response(206 if start else 200)
        self.send_header("ETag", '"benchmark-v1"')
        self.send_header("Content-Length", str(len(PAYLOAD) - start))
        if start:
            self.send_header("Content-Range", f"bytes {start}-{len(PAYLOAD)-1}/{len(PAYLOAD)}")
            self.server.range_requests += 1
        self.end_headers()
        end = len(PAYLOAD)
        if self.path == "/truncated" or (self.path == "/retry" and not start):
            end = 256 * 1024
            self.close_connection = True
        try:
            self.wfile.write(PAYLOAD[start:end])
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass  # Pause intentionally closes the current transfer.


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--output", type=Path, help="Copy result logs/JSON here after the test")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    scratch = prepare_workspace()
    compile_env = os.environ.copy()
    compile_env["MEOWODY_TEST_ROOT"] = str(root)
    compile_env["MEOWODY_TEST_OUTPUT"] = str(scratch)
    with (scratch / "compile.log").open("w", encoding="utf-8") as log:
        compiled = subprocess.run(["scons", "-f", "tests/SConstruct.service_perf"], cwd=root,
                                  env=compile_env, stdout=log, stderr=subprocess.STDOUT)
    if compiled.returncode:
        raise RuntimeError("\n".join((scratch / "compile.log").read_text(encoding="utf-8", errors="replace").splitlines()[-35:]))
    (scratch / "test.gdextension").write_text(
        '[configuration]\nentry_symbol="service_perf_init"\ncompatibility_minimum="4.7"\n'
        '[libraries]\nwindows="res://service_perf.dll"\n', encoding="utf-8")
    (scratch / ".godot").mkdir(exist_ok=True)
    (scratch / ".godot/extension_list.cfg").write_text("res://test.gdextension\n", encoding="utf-8")
    (scratch / "project.godot").write_text(
        'config_version=5\n[application]\nconfig/name="Service performance test"\n'
        '[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding="utf-8")
    (scratch / "test.gd").write_text('''extends SceneTree
func _initialize():
    call_deferred("run_tests")
func run_tests():
    var probe = ClassDB.instantiate("MeowodyServicePerfProbe")
    var passed = probe.run(OS.get_environment("MEOWODY_PERF_HTTP"), OS.get_environment("MEOWODY_PERF_SHA"))
    probe = null
    # Give the dummy audio mixer time to retire the benchmark's playbacks.
    await create_timer(0.25).timeout
    print("[ServicePerf] PASS" if passed else "[ServicePerf] FAILED")
    quit(0 if passed else 1)
''', encoding="utf-8")
    env = os.environ.copy()
    for name in ("profile", "temp"):
        (scratch / name).mkdir(exist_ok=True)
        (scratch / name / ".gdignore").touch()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(scratch / "profile")
    env["TEMP"] = env["TMP"] = str(scratch / "temp")
    server = ThreadingHTTPServer(("127.0.0.1", 0), DownloadHandler)
    server.range_requests = 0
    worker = Thread(target=server.serve_forever, daemon=True); worker.start()
    env["MEOWODY_PERF_HTTP"] = f"http://127.0.0.1:{server.server_port}"
    env["MEOWODY_PERF_SHA"] = hashlib.sha256(PAYLOAD).hexdigest()
    try:
        with (scratch / "runtime.log").open("w", encoding="utf-8") as log:
            result = subprocess.run([str(args.godot.resolve()), "--headless", "--audio-driver", "Dummy",
                                     "--path", str(scratch), "--script", "res://test.gd"],
                                    env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
    finally:
        server.shutdown(); server.server_close(); worker.join()
    output = (scratch / "runtime.log").read_text(encoding="utf-8", errors="replace")
    errors = [line for line in output.splitlines() if line.startswith(("ERROR:", "SCRIPT ERROR:")) and "root certificate store" not in line]
    print(output)
    if result.returncode or "[ServicePerf] PASS" not in output or errors or server.range_requests < 1:
        raise RuntimeError("Service performance/regression probe failed")

    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        for report in list(scratch.glob("*.log")) + list(scratch.glob("*.json")):
            shutil.copy2(report, args.output / report.name)


if __name__ == "__main__":
    main()
