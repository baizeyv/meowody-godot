"""Validate native template generation, then build it in the shared workspace."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from godot_test_workspace import prepare_workspace

ROOT = Path(__file__).resolve().parents[1]

def run(command, cwd=ROOT, env=None):
    result = subprocess.run([str(part) for part in command], cwd=cwd, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=900)
    if result.returncode:
        raise RuntimeError(result.stdout[-12000:] + result.stderr[-12000:])
    return result.stdout.strip()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--dependency", required=True, type=Path)
    args = parser.parse_args()
    godot, dependency = args.godot.resolve(), args.dependency.resolve()
    version = run([godot, "--version"])
    run(["scons", "-f", "tests/SConstruct.project_creator"])
    stage_root = ROOT / "build/project-creator"
    stage_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=stage_root) as temporary:
        stage = Path(temporary)
        source, generated = stage / "source", stage / "generated"
        ignore = shutil.ignore_patterns("*.obj", "*.dll", "*.pdb", "*.lib", "*.exp", "__pycache__", "node_modules", "dist")
        for directory in ("src", "reflect_codegen", "tools/godot_mcp", ".agents/skills/meowody-framework", "meow/addons", "meow/resources"):
            shutil.copytree(ROOT / directory, source / directory, ignore=ignore)
        for filename in ("SConstruct", ".gitignore", ".clang-format", "README.md", "framework-dependencies.json", "meow/bin/meowody.gdextension"):
            target = source / filename
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / filename, target)
        scratch = prepare_workspace()
        shutil.copytree(ROOT / "meow/addons", scratch / "addons", dirs_exist_ok=True)
        (scratch / "bin").mkdir()
        shutil.copy2(ROOT / "meow/bin/libmeowody.windows.template_debug.x86_64.dll", scratch / "bin")
        (scratch / "bin/meowody.gdextension").write_text('[configuration]\nentry_symbol="meowody_library_init"\ncompatibility_minimum="4.7"\n[libraries]\nwindows="res://bin/libmeowody.windows.template_debug.x86_64.dll"\n')
        config = (ROOT / "meow/project.godot").read_text(encoding="utf-8").replace('run/main_scene="res://project_creator/main.tscn"', '')
        (scratch / "project.godot").write_text(config, encoding="utf-8")
        environment = os.environ.copy()
        for folder, keys in (("profile", ("APPDATA", "LOCALAPPDATA")), ("temp", ("TEMP", "TMP"))):
            path = scratch / folder
            path.mkdir(); (path / ".gdignore").touch()
            environment.update({key: str(path) for key in keys})
        run([godot, "--headless", "--editor", "--path", scratch, "--import"], env=environment)
        output = run([godot, "--headless", "--path", scratch, "--script", "res://tests/project_creator/template_test.gd", "--", "--source", source, "--destination", generated, "--version", version], env=environment)
        if "[CreatorTemplate] failures=0" not in output or "ERROR:" in output:
            raise RuntimeError(output)
        print("[CreatorTemplate] PASS native source generation and overwrite protection", flush=True)
        scratch = prepare_workspace()
        shutil.rmtree(scratch)
        shutil.copytree(generated, scratch)
    config = json.loads((scratch / "meowody-project.json").read_text(encoding="utf-8"))
    run(["git", "init"], cwd=scratch)
    run(["git", "clone", "--no-hardlinks", dependency, scratch / "godot-cpp"], cwd=scratch)
    run(["git", "checkout", "--detach", config["godot_cpp_commit"]], cwd=scratch / "godot-cpp")
    run(["git", "submodule", "add", "--force", "https://github.com/godotengine/godot-cpp.git", "godot-cpp"], cwd=scratch)
    run(["git", "submodule", "absorbgitdirs", "godot-cpp"], cwd=scratch)
    native = ROOT / "build/project-creator/native-tests/creator_backend_test.exe"
    print(run([native, "--build", scratch, godot, version, config["godot_cpp_commit"]]), flush=True)

if __name__ == "__main__":
    main()
