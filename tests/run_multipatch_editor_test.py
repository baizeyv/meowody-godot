"""Exercise native MultiPatch, editor sidecars and SpriteAtlas interoperability."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time
from godot_test_workspace import prepare_workspace


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--visual", action="store_true")
    parser.add_argument("--rendering-method", default="gl_compatibility", choices=["gl_compatibility", "forward_plus", "mobile"])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    scratch = prepare_workspace()
    for name in ["meowody_atlas", "meowody_multipatch"]:
        shutil.copytree(root / "meow/addons" / name, scratch / "addons" / name)
    (scratch / "bin").mkdir()
    shutil.copy2(root / "meow/bin/libmeowody.windows.template_debug.x86_64.dll", scratch / "bin")
    (scratch / "bin/meowody.gdextension").write_text('''[configuration]
entry_symbol="meowody_library_init"
compatibility_minimum="4.7"
[libraries]
windows="res://bin/libmeowody.windows.template_debug.x86_64.dll"
''', encoding="utf-8")
    (scratch / "project.godot").write_text('''config_version=5
[application]
config/name="MultiPatch regression"
[display]
window/size/viewport_width=1200
window/size/viewport_height=800
[editor_plugins]
enabled=PackedStringArray("res://addons/meowody_atlas/plugin.cfg", "res://addons/meowody_multipatch/plugin.cfg")
[rendering]
renderer/rendering_method="gl_compatibility"
''', encoding="utf-8")
    (scratch / "profile").mkdir()
    (scratch / "profile/.gdignore").touch()
    environment = os.environ.copy()
    environment["APPDATA"] = environment["LOCALAPPDATA"] = str(scratch / "profile")
    (scratch / "temp").mkdir()
    (scratch / "temp/.gdignore").touch()
    environment["TEMP"] = environment["TMP"] = str(scratch / "temp")
    print("Scratch project:", scratch, flush=True)
    command = [str(args.godot.resolve()), "--editor", "--path", str(scratch), "--rendering-method", args.rendering_method]
    if not args.visual:
        command.append("--headless")
    log_path = scratch / "editor-test.log"
    with log_path.open("w", encoding="utf-8") as log:
        process = subprocess.Popen(command + ["--", "--test-multipatch"], stdout=log, stderr=subprocess.STDOUT, env=environment)
        deadline = time.monotonic() + 120
        while process.poll() is None:
            time.sleep(0.5)
            output = log_path.read_text(encoding="utf-8", errors="replace")
            if "SCRIPT ERROR" in output or time.monotonic() > deadline:
                process.terminate()
                break
        returncode = process.wait(timeout=10)
    output = log_path.read_text(encoding="utf-8", errors="replace")
    errors = [line for line in output.splitlines() if line.startswith("ERROR:") and "root certificate store" not in line]
    print("\n".join(line for line in output.splitlines() if "[MultiPatchTest]" in line))
    if returncode or "failures=0" not in output or "SCRIPT ERROR" in output or errors:
        print(output[-10000:])
        raise SystemExit(1)
    runtime = subprocess.run([str(args.godot.resolve()), "--headless", "--path", str(scratch), "--main-pack", str(scratch / "multipatch.pck"), "--script", "res://tests/meowody_multipatch/runtime_probe.gd"], capture_output=True, text=True, encoding="utf-8", env=environment, timeout=30)
    print(runtime.stdout)
    if runtime.returncode or "[MultiPatchTest] PASS" not in runtime.stdout:
        print(runtime.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
