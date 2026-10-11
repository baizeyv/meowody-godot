"""Exercise the real MCP stdio server against an isolated Godot editor."""
import argparse
import importlib.util
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
    parser.add_argument("--node", type=Path)
    args = parser.parse_args()
    node = str(args.node or shutil.which("node"))
    # Python wheel launchers depend on APPDATA; use the real binary in the isolated profile.
    wheel = importlib.util.find_spec("nodejs_wheel")
    if args.node is None and wheel and wheel.origin:
        binary = Path(wheel.origin).parent / "node.exe"
        if binary.exists():
            node = str(binary)
    root = Path(__file__).resolve().parents[1]
    scratch = prepare_workspace()
    for directory in ("addons", "bin"):
        shutil.copytree(root / "meow" / directory, scratch / directory, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns(".godot", "*.pdb", "*.lib", "*.exp"))
    helper = scratch / "addons/mcp_test_exit"
    helper.mkdir()
    (helper / "plugin.cfg").write_text('[plugin]\nname="MCP test exit"\nscript="plugin.gd"\n', encoding="utf-8")
    (helper / "plugin.gd").write_text('''@tool
extends EditorPlugin
func _process(_delta):
    if FileAccess.file_exists("res://.godot/mcp_test_complete"):
        get_tree().quit()
''', encoding="utf-8")
    plugins = [f"res://addons/{name}/plugin.cfg" for name in (
        "meowody_binding", "meowody_debug", "meowody_atlas", "meowody_multipatch",
        "meowody_nodes", "meowody_mcp", "mcp_test_exit")]
    plugin_list = ", ".join('"' + name + '"' for name in plugins)
    (scratch / "project.godot").write_text(f'''config_version=5
[application]
config/name="MCP regression"
run/main_scene="res://scene.tscn"
[editor_plugins]
enabled=PackedStringArray({plugin_list})
[debug]
file_logging/enable_file_logging=true
[rendering]
renderer/rendering_method="gl_compatibility"
''', encoding="utf-8")
    (scratch / "scene.tscn").write_text('''[gd_scene format=3]
[node name="Main" type="Control"]
[node name="Title" type="Label" parent="."]
text="MCP regression"
''', encoding="utf-8")
    (scratch / "binding.tscn").write_text('[gd_scene format=3]\n[node name="Binding" type="TttView"]\n', encoding="utf-8")
    (scratch / "sprite.tres").write_text('[gd_resource type="MultiPatchSprite" format=3]\n[resource]\n', encoding="utf-8")
    env = os.environ.copy()
    for directory in ("profile", "temp"):
        (scratch / directory).mkdir()
        (scratch / directory / ".gdignore").touch()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(scratch / "profile")
    env["TEMP"] = env["TMP"] = str(scratch / "temp")
    if os.name == "nt":
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x0002 | 0x8000)
    command = [str(args.godot.resolve()), "--editor", "--path", str(scratch)]
    if not args.visual:
        command.append("--headless")
    endpoint = scratch / ".godot/meowody_mcp/bridge.json"
    with (scratch / "editor.log").open("w", encoding="utf-8") as log:
        editor = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 60
            while not endpoint.exists():
                output = (scratch / "editor.log").read_text(encoding="utf-8", errors="replace")
                if editor.poll() is not None or "SCRIPT ERROR:" in output or time.monotonic() >= deadline:
                    raise RuntimeError(output[-10000:])
                time.sleep(0.2)
            client = [node, str(root / "tests/mcp_client_test.mjs"), str(scratch)]
            if args.visual:
                client.append("--visual")
            with (scratch / "client.log").open("w", encoding="utf-8") as client_log:
                result = subprocess.run(client, cwd=root, env=env, stdout=client_log,
                                        stderr=subprocess.STDOUT, timeout=120)
            if result.returncode:
                raise RuntimeError((scratch / "client.log").read_text(encoding="utf-8", errors="replace"))
            (scratch / ".godot/mcp_test_complete").touch()
            if editor.wait(timeout=30):
                raise RuntimeError("Godot editor failed on exit")
            if endpoint.exists():
                raise RuntimeError("Editor endpoint was not removed on plugin shutdown")
            subprocess.run([node, str(root / "tests/mcp_client_test.mjs"), str(scratch), "--offline"],
                           cwd=root, env=env, check=True, timeout=15)
        finally:
            if editor.poll() is None:
                editor.terminate()
                editor.wait(timeout=10)
    output = (scratch / "editor.log").read_text(encoding="utf-8", errors="replace")
    errors = [line for line in output.splitlines() if line.startswith(("SCRIPT ERROR:", "ERROR:", "SHADER ERROR:"))
              and "root certificate store" not in line and
              not (not args.visual and 'Parameter "t" is null' in line)]
    if errors or "NATIVE CRASH" in output:
        raise RuntimeError(output[-18000:])
    print((scratch / "client.log").read_text(encoding="utf-8", errors="replace"))
    print("[MCPEditorTest] PASS:", scratch)


if __name__ == "__main__":
    main()
