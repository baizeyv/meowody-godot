"""Build and run async/input centers tests in an isolated Windows Godot project."""
import argparse
import os
from pathlib import Path
import subprocess
from godot_test_workspace import prepare_workspace


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    scratch = prepare_workspace()
    compile_env = os.environ.copy()
    compile_env["MEOWODY_TEST_ROOT"] = str(root)
    compile_env["MEOWODY_TEST_OUTPUT"] = str(scratch)
    command = ["scons", "-f", "tests/SConstruct.centers"]
    with (scratch / "compile.log").open("w", encoding="utf-8") as log:
        compiled = subprocess.run(command, cwd=root, env=compile_env, stdout=log, stderr=subprocess.STDOUT)
    if compiled.returncode:
        raise RuntimeError((scratch / "compile.log").read_text(encoding="utf-8", errors="replace"))
    (scratch / "test.gdextension").write_text(
        '[configuration]\nentry_symbol="centers_test_init"\ncompatibility_minimum="4.7"\n'
        '[libraries]\nwindows="res://centers_test.dll"\n', encoding="utf-8")
    (scratch / ".godot").mkdir()
    (scratch / ".godot/extension_list.cfg").write_text("res://test.gdextension\n", encoding="utf-8")
    (scratch / "main.tscn").write_text('\n[gd_scene format=3]\n[node name="Main" type="Node"]\n', encoding="utf-8")
    (scratch / "project.godot").write_text(
        'config_version=5\n[application]\nconfig/name="Centers test"\nrun/main_scene="res://main.tscn"\n'
        '[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding="utf-8")
    (scratch / "test.gd").write_text('''extends SceneTree
func _initialize():
    call_deferred("run_tests")
func run_tests():
    var probe = ClassDB.instantiate("MeowodyCentersProbe")
    for phase in range(4):
        if not probe.phase(phase):
            push_error("Centers phase failed: %d" % phase)
            quit(1)
            return
        await process_frame
        await process_frame
    probe = null
    print("[CentersTest] PASS")
    quit(0)
''', encoding="utf-8")
    env = os.environ.copy()
    for name in ("profile", "temp"):
        (scratch / name).mkdir()
        (scratch / name / ".gdignore").touch()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(scratch / "profile")
    env["TEMP"] = env["TMP"] = str(scratch / "temp")
    with (scratch / "runtime.log").open("w", encoding="utf-8") as log:
        result = subprocess.run([str(args.godot.resolve()), "--headless", "--path", str(scratch),
                                 "--script", "res://test.gd"], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=60)
    output = (scratch / "runtime.log").read_text(encoding="utf-8", errors="replace")
    errors = [line for line in output.splitlines() if line.startswith(("ERROR:", "SCRIPT ERROR:")) and "root certificate store" not in line]
    if result.returncode or "[CentersTest] PASS" not in output or errors:
        raise RuntimeError(output)
    print("[CentersTest] PASS:", scratch)


if __name__ == "__main__":
    main()
