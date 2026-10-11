"""Run the real editor/build/reload workflow in a disposable copy of the project."""
import argparse
import os
import shutil
import site
import subprocess
import sys
from godot_test_workspace import prepare_workspace
from pathlib import Path


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--suite", choices=("binding", "debug", "crash"), default="binding")
    args = parser.parse_args()
    if args.suite == "crash" and os.name != "nt":
        parser.error("The native crash regression currently targets Windows/MSVC.")
    root = Path(__file__).resolve().parents[1]
    (root / "build").mkdir(exist_ok=True)
    scratch = prepare_workspace().parent
    print(f"Scratch project and logs: {scratch}", flush=True)
    for directory in ("src", "reflect_codegen", "meow"):
        shutil.copytree(
            root / directory, scratch / directory,
            dirs_exist_ok=True,
            ignore=shutil.ignore_patterns(".godot", "__pycache__", "*.bak"),
        )
    shutil.copy2(root / "SConstruct", scratch / "SConstruct")
    staged = root / "meow/.godot/binding-build"
    for library in staged.glob("libmeowody.*"):
        live = root / "meow/bin" / library.name
        if library.suffix in (".dll", ".so") and (not live.exists() or library.stat().st_mtime > live.stat().st_mtime):
            shutil.copy2(library, scratch / "meow/bin" / library.name)
            pdb = library.with_suffix(".pdb")
            if pdb.exists():
                shutil.copy2(pdb, scratch / "meow/bin" / pdb.name)
    if (root / ".sconsign.dblite").exists():
        shutil.copy2(root / ".sconsign.dblite", scratch / ".sconsign.dblite")
    # Reuse the already-built godot-cpp dependency; generated project files are isolated.
    if os.name == "nt":
        subprocess.run(
            ["cmd", "/c", "mklink", "/J", str(scratch / "godot-cpp"), str(root / "godot-cpp")],
            check=True, capture_output=True,
        )
    else:
        (scratch / "godot-cpp").symlink_to(root / "godot-cpp", target_is_directory=True)
    environment = os.environ.copy()
    if os.name == "nt":
        import ctypes
        # Only this test process and its children: unexpected faults must not leave a WER dialog.
        ctypes.windll.kernel32.SetErrorMode(0x0002 | 0x8000)
        (scratch / "profile").mkdir()
        environment["APPDATA"] = environment["LOCALAPPDATA"] = str(scratch / "profile")
        (scratch / "temp").mkdir()
        environment["TEMP"] = environment["TMP"] = str(scratch / "temp")
        environment["PYTHONUSERBASE"] = site.USER_BASE
    if args.suite == "crash":
        fixture = scratch / "src/crash_test"
        fixture.mkdir()
        for filename in ("CrashProbe.h", "CrashProbe.cpp"):
            shutil.copy2(root / "tests/fixtures" / filename, fixture / filename)
        shutil.copy2(root / "tests/fixtures/crash_probe.gd", scratch / "meow/crash_probe.gd")
        environment.pop("MEOWODY_BINDING_BUILD", None)
        environment.pop("MEOWODY_BINDING_OUTPUT", None)
        with (scratch / "crash-build.log").open("w") as log:
            build = subprocess.run(["scons"], cwd=scratch, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=600)
        with (scratch / "crash-build.log").open("rb") as log:
            log.seek(max(0, (scratch / "crash-build.log").stat().st_size - 8192))
            print("\n".join(log.read().decode("utf-8", errors="replace").splitlines()[-20:]), flush=True)
        if build.returncode:
            raise SystemExit(build.returncode)
        with (scratch / "crash-process.log").open("w") as log:
            crashed = subprocess.run([str(args.godot.resolve()), "--headless", "--path", str(scratch / "meow"),
                "--script", "res://crash_probe.gd"], stdout=log, stderr=subprocess.STDOUT, env=environment, timeout=45)
        reports = list((scratch / "profile").rglob("crash_*.log"))
        if crashed.returncode == 0 or not reports:
            raise SystemExit("Expected a real process crash and a persisted report. See crash-process.log.")
        report = reports[-1].read_text(encoding="utf-8", errors="replace")
        for expected in ("0xC0000005", "ACCESS_VIOLATION", "write address 0x18", "CrashProbe.cpp:", "crash_probe_fault_site", "CrashProbe::crash", "#1 ", "CrashProbeMarker"):
            if expected not in report:
                print(report)
                raise SystemExit(f"Missing crash evidence: {expected}")
        print(f"[CrashTest] PASS exception, fault address, native function, source line and recent logs: {reports[-1]}")
        return
    flag = "--test-binding-editor" if args.suite == "binding" else "--debug-console-editor-test"
    with (scratch / "editor-test.log").open("w", encoding="utf-8") as log:
        completed = subprocess.run(
            [str(args.godot.resolve()), "--headless", "--editor", "--path", str(scratch / "meow"),
             "--", flag],
            stdout=log, stderr=subprocess.STDOUT, timeout=600, env=environment,
        )
    output = (scratch / "editor-test.log").read_text(encoding="utf-8", errors="replace")
    print("\n".join(line for line in output.splitlines() if "[BindingTest]" in line or "[DebugEditorTest]" in line), flush=True)
    print(f"Godot exit code: {completed.returncode}", flush=True)
    if completed.returncode:
        print("\n".join(output.splitlines()[-35:]), flush=True)
        raise SystemExit(completed.returncode)
    if "failures=0" not in output:
        raise SystemExit("Editor did not finish the regression test.")
    if any(error in output for error in ("SCRIPT ERROR", "The tree cannot", "Unable to recreate GDExtension instance")):
        raise SystemExit("Editor reported a script, Tree, or GDExtension reload error. See editor-test.log.")


if __name__ == "__main__":
    main()
