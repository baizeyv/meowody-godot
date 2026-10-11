"""Run isolated native-node editor/export/runtime checks; optional real GPU screenshots."""
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
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    scratch = prepare_workspace()
    shutil.copytree(root / "meow/addons/meowody_nodes", scratch / "addons/meowody_nodes", dirs_exist_ok=True)
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
config/name="Common Nodes regression"
[display]
window/size/viewport_width=1200
window/size/viewport_height=900
[input_devices]
pointing/emulate_mouse_from_touch=false
[editor_plugins]
enabled=PackedStringArray("res://addons/meowody_nodes/plugin.cfg")
[rendering]
renderer/rendering_method="gl_compatibility"
''', encoding="utf-8")
    (scratch / "profile").mkdir()
    (scratch / "profile/.gdignore").touch()
    env = os.environ.copy()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(scratch / "profile")
    (scratch / "temp").mkdir()
    (scratch / "temp/.gdignore").touch()
    env["TEMP"] = env["TMP"] = str(scratch / "temp")
    command = [str(args.godot.resolve()), "--path", str(scratch)]
    print("Scratch:", scratch, flush=True)
    def run(stage, arguments, require_marker=True):
        log_path = scratch / (stage + ".log")
        with log_path.open("w", encoding="utf-8") as log:
            process = subprocess.Popen(command + arguments, stdout=log, stderr=subprocess.STDOUT, env=env)
            deadline = time.monotonic() + 90
            while process.poll() is None:
                time.sleep(0.25)
                recent = log_path.read_text(encoding="utf-8", errors="replace")
                if "SCRIPT ERROR:" in recent or time.monotonic() > deadline:
                    process.terminate()
                    break
            returncode = process.wait(timeout=10)
        output = log_path.read_text(encoding="utf-8", errors="replace")
        print("\n".join(line for line in output.splitlines() if "[NodesTest]" in line))
        errors = [line for line in output.splitlines() if line.startswith(("ERROR:", "SCRIPT ERROR:", "SHADER ERROR:")) and "root certificate store" not in line]
        if returncode or (require_marker and "failures=0" not in output) or errors:
            print(output[-14000:])
            raise SystemExit(1)
    run("editor", ["--headless", "--editor", "--", "--test-common-nodes"])
    (scratch / "export_presets.cfg").write_text('''[preset.0]
name="Selected"
platform="Windows Desktop"
runnable=true
export_filter="resources"
export_files=PackedStringArray("res://native_nodes.tscn", "res://tests/meowody_nodes/runtime_test.gd", "res://addons/meowody_nodes/examples/gallery.tscn", "res://bin/meowody.gdextension")
include_filter=""
exclude_filter=""
export_path="nodes.pck"
[preset.0.options]
binary_format/architecture="x86_64"
''', encoding="utf-8")
    run("selected-export", ["--headless", "--editor", "--export-pack", "Selected", str(scratch / "nodes.pck")], require_marker=False)
    flags = [] if args.visual else ["--headless"]
    run("runtime", flags + ["--main-pack", str(scratch / "nodes.pck"), "--script", "res://tests/meowody_nodes/runtime_test.gd"])


if __name__ == "__main__":
    main()
