"""Exercise real project generation/build in the shared Godot test workspace."""
import argparse
from pathlib import Path
import shutil
import sys
import tempfile
from unittest.mock import patch

from godot_test_workspace import prepare_workspace

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/project_creator"))
from creator import build_and_verify, copy_source, generate_project, inspect_engine, prepare_dependency, run


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--dependency", required=True, type=Path, help="Clean checkout of the pinned official godot-cpp commit")
    args = parser.parse_args()
    scratch = prepare_workspace()
    # prepare_workspace guarantees this is the disposable project, not the user's project.
    shutil.rmtree(scratch)
    snapshot_root = ROOT / "build/project-creator"
    snapshot_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=snapshot_root) as temporary:
        source = Path(temporary)
        for directory in ("src", "reflect_codegen", "tools/godot_mcp", ".agents/skills/meowody-framework", "meow/addons"):
            copy_source(ROOT / directory, source / directory)
        for filename in ("SConstruct", ".gitignore", ".clang-format", "README.md", "framework-dependencies.json", "meow/bin/meowody.gdextension"):
            target = source / filename
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / filename, target)
        version, api_version = inspect_engine(args.godot)
        dependency = generate_project(source, scratch, "创建器 Test", version, api_version, "local-validation")
        run(["git", "init", source], source.parent, log=lambda _: None)
        def local_checkout(_url, ref, destination, log):
            run(["git", "clone", "--no-hardlinks", args.dependency.resolve(), destination], destination.parent, log)
            run(["git", "checkout", "--detach", ref], destination, log)
        with patch("creator.checkout", side_effect=local_checkout):
            prepare_dependency(source, scratch, dependency, log=lambda _: None)
    log_path = ROOT / "build/test-results/project-creator.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log:
        def report(message):
            log.write(message + "\n")
            log.flush()
            if "error" in message.lower() or "[Creator]" in message or message.startswith("Building"):
                print(message, flush=True)
        build_and_verify(scratch, args.godot.resolve(), report)
    print("[Creator] PASS source generation, real build, editor import and runtime:", scratch)


if __name__ == "__main__":
    main()
