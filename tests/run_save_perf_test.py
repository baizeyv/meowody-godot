"""Build and run save copy performance tests in an isolated Windows Godot project."""
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
    command = ["scons", "-f", "tests/SConstruct.save_perf"]
    with (scratch / "compile.log").open("w", encoding="utf-8") as log:
        compiled = subprocess.run(command, cwd=root, env=compile_env, stdout=log, stderr=subprocess.STDOUT)
    if compiled.returncode:
        raise RuntimeError((scratch / "compile.log").read_text(encoding="utf-8", errors="replace"))
    (scratch / "test.gdextension").write_text(
        '[configuration]\nentry_symbol="save_perf_init"\ncompatibility_minimum="4.7"\n'
        '[libraries]\nwindows="res://save_perf_test.dll"\n', encoding="utf-8")
    (scratch / ".godot").mkdir()
    (scratch / ".godot/extension_list.cfg").write_text("res://test.gdextension\n", encoding="utf-8")
    (scratch / "main.tscn").write_text('\n[gd_scene format=3]\n[node name="Main" type="Node"]\n', encoding="utf-8")
    (scratch / "project.godot").write_text(
        'config_version=5\n[application]\nconfig/name="Model save test"\nrun/main_scene="res://main.tscn"\n'
        '[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding="utf-8")
    env = os.environ.copy()
    for name in ("profile", "temp"):
        (scratch / name).mkdir()
        (scratch / name / ".gdignore").touch()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(scratch / "profile")
    env["TEMP"] = env["TMP"] = str(scratch / "temp")
    with (scratch / "runtime.log").open("w", encoding="utf-8") as log:
        result = subprocess.run([str(args.godot.resolve()), "--headless", "--path", str(scratch),
                                 "--quit-after", "2"], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=60)
    output = (scratch / "runtime.log").read_text(encoding="utf-8", errors="replace")
    errors = [line for line in output.splitlines() if line.startswith(("ERROR:", "SCRIPT ERROR:")) and "root certificate store" not in line]
    if result.returncode or "[SavePerfTest] PASS" not in output or errors:
        raise RuntimeError(output)
    print("[SavePerfTest] PASS:", scratch)
    print("\n".join(line for line in output.splitlines() if "[SavePerf]" in line))


if __name__ == "__main__":
    main()
