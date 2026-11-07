"""Exercise the actual native creator in the isolated shared Godot workspace."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

from godot_test_workspace import prepare_workspace


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--visual", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    scratch = prepare_workspace()
    shutil.copytree(root / "meow/addons", scratch / "addons", dirs_exist_ok=True)
    shutil.copytree(root / "meow/project_creator", scratch / "project_creator",
                    ignore=shutil.ignore_patterns("__pycache__"))
    (scratch / "bin").mkdir()
    shutil.copy2(root / "meow/bin/libmeowody.windows.template_debug.x86_64.dll", scratch / "bin")
    (scratch / "bin/meowody.gdextension").write_text('''[configuration]
entry_symbol="meowody_library_init"
compatibility_minimum="4.7"
[libraries]
windows="res://bin/libmeowody.windows.template_debug.x86_64.dll"
''', encoding="utf-8")
    config = (root / "meow/project.godot").read_text(encoding="utf-8")
    config = config.replace('config/name="Meowody Project Creator"', 'config/name="Creator UI regression"')
    (scratch / "project.godot").write_text(config, encoding="utf-8")
    environment = os.environ.copy()
    for folder, keys in (("profile", ("APPDATA", "LOCALAPPDATA")), ("temp", ("TEMP", "TMP"))):
        directory = scratch / folder
        directory.mkdir()
        (directory / ".gdignore").touch()
        environment.update({key: str(directory) for key in keys})
    command = [str(args.godot.resolve()), "--path", str(scratch)]
    log_dir = root / "build/test-results"
    log_dir.mkdir(parents=True, exist_ok=True)
    for stage, flags in (("import", ([] if args.visual else ["--headless"]) + ["--editor", "--import"]),
                         ("runtime", ([] if args.visual else ["--headless"]) + ["--script", "res://tests/project_creator/ui_test.gd"])):
        with (log_dir / f"creator-ui-{stage}.log").open("w", encoding="utf-8") as log:
            result = subprocess.run(command + flags, stdout=log, stderr=subprocess.STDOUT, env=environment, timeout=120)
        output = (log_dir / f"creator-ui-{stage}.log").read_text(encoding="utf-8", errors="replace")
        errors = [line for line in output.splitlines() if line.startswith(("ERROR:", "SCRIPT ERROR:")) and "root certificate store" not in line]
        if result.returncode or errors or (stage == "runtime" and "[CreatorUI] failures=0" not in output):
            raise RuntimeError(output[-10000:])
    if args.visual:
        shutil.copy2(scratch / "creator-ui.png", log_dir / "creator-ui.png")
        shutil.copy2(scratch / "creator-engines.png", log_dir / "creator-engines.png")
        shutil.copy2(scratch / "creator-folder-dialog.png", log_dir / "creator-folder-dialog.png")
        shutil.copy2(scratch / "creator-framework-refs.png", log_dir / "creator-framework-refs.png")
        for screenshot in scratch.glob("creator-resize-*.png"):
            shutil.copy2(screenshot, log_dir / screenshot.name)
    print("[CreatorUI] PASS native UI, worker failure recovery, directory protection, preferences and quit guard")


if __name__ == "__main__":
    main()
