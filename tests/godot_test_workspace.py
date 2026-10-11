"""One disposable Godot workspace for all suites. Run suites serially."""
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT / "tests/workspace"


def prepare_workspace():
    if WORKSPACE.is_symlink() or os.path.isjunction(WORKSPACE):
        raise ValueError("Test workspace must be a real directory")
    # Binding tests link the shared dependency. Remove the link, never its target.
    dependency = WORKSPACE / "godot-cpp"
    if dependency.is_symlink():
        dependency.unlink()
    elif os.path.isjunction(dependency):
        dependency.rmdir()
    if WORKSPACE.exists():
        shutil.rmtree(WORKSPACE)
    project = WORKSPACE / "meow"
    project.mkdir(parents=True)
    shutil.copytree(ROOT / "tests/fixtures", project / "tests")
    shutil.copytree(ROOT / "meow/addons/meowody_nodes/shaders",
                    project / "addons/meowody_nodes/shaders")
    return project


if __name__ == "__main__":
    first = prepare_workspace()
    (first / "stale.txt").touch()
    second = prepare_workspace()
    assert first == second and not (second / "stale.txt").exists()
    assert (second / "tests/meowody_nodes/editor_test.gd").is_file()
    print("Shared Godot workspace: PASS")
