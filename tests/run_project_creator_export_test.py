"""Export and launch the actual Debug and Release templates in an isolated profile."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess

from godot_test_workspace import prepare_workspace


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    godot = args.godot.resolve()
    version = subprocess.run([str(godot), "--version"], capture_output=True, text=True, check=True).stdout.strip()
    match = re.fullmatch(r"(4\.\d+(?:\.\d+)?)\.stable\.official\.[0-9a-f]+", version)
    if not match:
        raise ValueError("Use the official stable editor matching the installed export templates")
    template_version = match[1] + ".stable"
    templates = Path(os.environ["APPDATA"]) / "Godot/export_templates" / template_version
    profile = root / "build/creator-export-profile"
    isolated_templates = profile / "Godot/export_templates" / template_version
    isolated_templates.mkdir(parents=True, exist_ok=True)
    for name in ("version.txt", "windows_debug_x86_64.exe", "windows_release_x86_64.exe",
                 "windows_debug_x86_64_console.exe", "windows_release_x86_64_console.exe"):
        shutil.copy2(templates / name, isolated_templates / name)
    scratch = prepare_workspace()
    shutil.copytree(root / "meow", scratch, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns(".godot", "__pycache__", "*.pdb", "*.lib", "*.exp", "~*"))
    extension = scratch / "bin/meowody.gdextension"
    extension.write_text(extension.read_text(encoding="utf-8").replace("reloadable = true", "reloadable = false"), encoding="utf-8")
    env = os.environ.copy()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(profile)
    logs = root / "build/test-results"
    logs.mkdir(parents=True, exist_ok=True)
    for mode in ("debug", "release"):
        folder = root / ("build/project-creator/windows-x64" if mode == "release" else "build/project-creator/windows-x64-debug")
        folder.mkdir(parents=True, exist_ok=True)
        executable = folder / "MeowodyProjectCreator.exe"
        stages = (
            ("export", [str(godot), "--headless", "--editor", "--path", str(scratch), "--export-" + mode, "Windows Desktop", str(executable)]),
            ("runtime", [str(executable), "--headless", "--quit-after", "5"]),
        )
        for stage, command in stages:
            log_path = logs / f"creator-{mode}-{stage}.log"
            with log_path.open("w", encoding="utf-8") as stream:
                result = subprocess.run(command, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=120)
            output = log_path.read_text(encoding="utf-8", errors="replace")
            errors = [line for line in output.splitlines() if line.startswith(("ERROR:", "SCRIPT ERROR:"))]
            if result.returncode or errors or (stage == "runtime" and "[Runtime] startup" not in output):
                raise RuntimeError(f"{mode} {stage} failed ({result.returncode}):\n{output[-7000:]}")
            print(f"[CreatorExport] {mode} {stage}: PASS", flush=True)
        if "binary_format/embed_pck=false" in (scratch / "export_presets.cfg").read_text(encoding="utf-8"):
            if not executable.with_suffix(".pck").is_file():
                raise RuntimeError("External PCK is missing from the export")
        if not (folder / f"libmeowody.windows.template_{mode}.x86_64.dll").is_file():
            raise RuntimeError("Exported native extension is missing")
    print("[CreatorExport] PASS both official templates, resource packaging and native extensions", flush=True)


if __name__ == "__main__":
    main()
