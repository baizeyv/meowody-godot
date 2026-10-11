"""Exercise real Godot imports and atlas lifecycle in an isolated project."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time
from godot_test_workspace import prepare_workspace


def unexpected_errors(output):
    allowed = ("Failed to read the root certificate store.", "[UIAtlas] res://art/icons/huge.png",
               "[UIAtlas] res://art/icons/red.png already belongs")
    return [line for line in output.splitlines() if line.startswith("ERROR:")
            and not any(message in line for message in allowed)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--visual", action="store_true")
    parser.add_argument("--renderer", default="gl_compatibility")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    scratch = prepare_workspace()
    addon = scratch / "addons/meowody_atlas"
    shutil.copytree(root / "meow/addons/meowody_atlas", addon)
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
config/name="Atlas regression"
[display]
window/size/viewport_width=1200
window/size/viewport_height=800
[editor_plugins]
enabled=PackedStringArray("res://addons/meowody_atlas/plugin.cfg")
[rendering]
renderer/rendering_method="gl_compatibility"
'''.replace('gl_compatibility', args.renderer), encoding="utf-8")
    environment = os.environ.copy()
    (scratch / "profile").mkdir()
    (scratch / "profile/.gdignore").touch()
    environment["APPDATA"] = str(scratch / "profile")
    environment["LOCALAPPDATA"] = str(scratch / "profile")
    (scratch / "temp").mkdir()
    (scratch / "temp/.gdignore").touch()
    environment["TEMP"] = environment["TMP"] = str(scratch / "temp")
    print("Scratch project:", scratch, flush=True)
    command = [str(args.godot.resolve()), "--editor", "--path", str(scratch)]
    if not args.visual:
        command.append("--headless")
    with (scratch / "editor-test.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen(command + ["--", "--test-ui-atlas"], stdout=log,
                                   stderr=subprocess.STDOUT, env=environment)
        deadline = time.monotonic() + 240
        while process.poll() is None:
            time.sleep(0.5)
            text = (scratch / "editor-test.log").read_text(encoding="utf-8", errors="replace")
            if "SCRIPT ERROR" in text or time.monotonic() > deadline:
                process.terminate()
                break
        returncode = process.wait(timeout=10)
    output = (scratch / "editor-test.log").read_text(encoding="utf-8")
    print("\n".join(line for line in output.splitlines() if "[AtlasTest]" in line))
    if returncode or "failures=0" not in output or "SCRIPT ERROR" in output or unexpected_errors(output):
        print("\n".join(output.splitlines()[-80:]))
        raise SystemExit(returncode or 1)
    print("Godot editor exited cleanly.")
    for filename, flags in [("atlas-test.pck", []), ("atlas-android.pck", ["--", "--android"])]:
        runtime = subprocess.run([str(args.godot.resolve()), "--headless", "--path", str(scratch),
                                  "--main-pack", str(scratch / filename), "--script",
                                  "res://tests/meowody_atlas/runtime_probe.gd"] + flags,
                                 capture_output=True, text=True, env=environment, timeout=30)
        print(runtime.stdout)
        if runtime.returncode or "[AtlasTest] PASS" not in runtime.stdout:
            print(runtime.stderr)
            raise SystemExit(runtime.returncode or 1)
    # A clean checkout must work without sharing any editor/import cache.
    fresh = scratch
    shutil.rmtree(scratch / ".godot")
    shutil.rmtree(scratch / "profile")
    (scratch / "profile").mkdir()
    (scratch / "profile/.gdignore").touch()
    cold = subprocess.run([str(args.godot.resolve()), "--headless", "--editor", "--path", str(fresh),
                           "--", "--test-ui-atlas-fresh"], capture_output=True, text=True,
                          env=environment, timeout=60)
    (fresh / "fresh-editor-test.log").write_text(cold.stdout + cold.stderr, encoding="utf-8")
    print("\n".join(line for line in cold.stdout.splitlines() if "[AtlasTest]" in line))
    if cold.returncode or "failures=0" not in cold.stdout or "SCRIPT ERROR" in cold.stderr or unexpected_errors(cold.stdout + cold.stderr):
        print(cold.stdout[-6000:] + cold.stderr[-6000:])
        raise SystemExit(cold.returncode or 1)


if __name__ == "__main__":
    main()
