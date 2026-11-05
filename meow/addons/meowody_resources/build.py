"""Isolated, native Godot content export. Requires only Python's standard library."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def run(godot, stage, log, *args):
    with log.open("a", encoding="utf-8") as stream:
        process = subprocess.Popen([str(godot), "--headless", "--path", str(stage), *args], stdout=stream, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 900
        marker = read(stage / ".resource_build_context.json").get("cancel", "")
        try:
            while process.poll() is None:
                if marker and Path(marker).exists(): raise RuntimeError("Resource build cancelled")
                if time.monotonic() > deadline: raise TimeoutError(f"Content build timed out; see {log}")
                if "SCRIPT ERROR:" in log.read_text(encoding="utf-8", errors="replace"): raise RuntimeError(f"Editor script failed; see {log}")
                time.sleep(0.1)
        finally:
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=10)
                except subprocess.TimeoutExpired: process.kill(); process.wait()
    if process.returncode:
        raise RuntimeError(f"Godot content build failed ({process.returncode}); see {log}")


def build(args):
    project = args.project.resolve()
    settings = read(project / "resources/settings.json")
    profile_name = args.profile or settings["active_profile"]
    profile = settings["profiles"][profile_name]
    output = (project / profile["output"]).resolve() / args.platform
    # A reserved build workspace, outside the Godot project and never reused by tests.
    if output == project or project in output.parents:
        raise ValueError("Content output must be outside the Godot project")
    output.mkdir(parents=True, exist_ok=True)
    stage = output / ".staging"
    if stage.exists():
        if stage.is_symlink() or stage.is_junction() or stage.resolve().parent != output:
            raise ValueError("Unsafe staging directory")
        shutil.rmtree(stage)
    shutil.copytree(project, stage, ignore=shutil.ignore_patterns(".godot", ".git", "*.pck", ".resource_export_job.json"))
    write(stage / ".resource_build_context.json", {"cancel": str(project / ".godot/meowody_resources/cancel.json")})
    # Build workers enable only content-related plugins and never run binding/reload/MCP.
    project_file = stage / "project.godot"
    text = project_file.read_text(encoding="utf-8")
    import re
    plugins = [f"res://addons/{name}/plugin.cfg" for name in ("meowody_atlas", "meowody_nodes", "meowody_resources") if (stage / "addons" / name / "plugin.cfg").is_file()]
    text = re.sub(r"enabled=PackedStringArray\([^\n]*\)", "enabled=PackedStringArray(" + ", ".join(json.dumps(name) for name in plugins) + ")", text)
    if "[editor_plugins]" not in text:
        text += '\n[editor_plugins]\nenabled=PackedStringArray("res://addons/meowody_resources/plugin.cfg")\n'
    project_file.write_text(text, encoding="utf-8")
    # Preserve platform options, but force a complete file walk in the staging preset.
    presets = stage / "export_presets.cfg"
    preset_name = profile["presets"][args.platform]
    if not presets.exists() or f'name="{preset_name}"' not in presets.read_text(encoding="utf-8"):
        platform = {"windows": "Windows Desktop", "android": "Android", "ios": "iOS"}[args.platform]
        presets.write_text(f'[preset.0]\nname="{preset_name}"\nplatform="{platform}"\nexport_filter="all_resources"\ninclude_filter=""\nexclude_filter=""\n[preset.0.options]\ntexture_format/bptc=true\n', encoding="utf-8")
    else:
        presets.write_text(re.sub(r'export_filter="[^"]*"', 'export_filter="all_resources"', presets.read_text(encoding="utf-8")), encoding="utf-8")
    log = output / "build.log"
    log.write_text("", encoding="utf-8")
    analysis_path = output / "analysis.json"
    analysis_path.unlink(missing_ok=True)
    run(args.godot, stage, log, "--editor", "--", "--resource-analyze", str(analysis_path))
    analysis = read(analysis_path)
    if not analysis or analysis.get("errors"):
        raise ValueError("Resource analysis failed: " + "; ".join(analysis.get("errors", [f"see {log}"])))
    if args.analyze:
        return analysis
    previous = read(output / "build_state.json")
    code_files = sorted(p for p in stage.rglob("*") if p.is_file() and p.suffix in (".gd", ".gdshader", ".gdshaderinc", ".gdextension", ".dll", ".so", ".dylib") and ".godot" not in p.parts)
    compatibility = settings["compatibility"] + ":" + hashlib.sha256(json.dumps([(str(p.relative_to(stage)), digest(p)) for p in code_files]).encode()).hexdigest()
    version_info = subprocess.check_output([str(args.godot), "--version"], text=True).strip()
    base = {"engine": version_info, "platform": args.platform, "preset": digest(presets), "compatibility": compatibility,
            "builder": digest(Path(__file__)), "analyzer": digest(stage / "addons/meowody_resources/builder.gd")}
    packs, states, changed, reused = {}, {}, [], []
    for pack_id, pack in sorted(analysis["packs"].items()):
        files = {}
        for path in pack["files"]:
            source = stage / path.removeprefix("res://")
            files[path] = digest(source)
            imported = source.with_name(source.name + ".import")
            if imported.exists():
                files[path + ".import"] = digest(imported)
                # Imported payloads are part of the fingerprint, including atlas outputs.
                for target in re.findall(r'"(res://\.godot/[^"\n]+)"', imported.read_text(encoding="utf-8")):
                    artifact = stage / target.removeprefix("res://")
                    if artifact.exists(): files[target] = digest(artifact)
        fingerprint = hashlib.sha256(json.dumps({"base": base, "files": files, "dependencies": pack["dependencies"]}, sort_keys=True).encode()).hexdigest()
        old = previous.get("packs", {}).get(pack_id, {})
        old_path = output / old.get("file", "missing")
        if old.get("fingerprint") == fingerprint and old_path.is_file() and digest(old_path) == old.get("sha256"):
            record = {key: old[key] for key in ("file", "sha256", "size")}
            reused.append(pack_id)
        else:
            temporary = output / f"{pack_id}.pending.pck"
            # GDScript's own exporter emits bytecode before other plugins see the file.
            # Exclude code in the preset, before any export plugin can transform it.
            preset_text = presets.read_text(encoding="utf-8")
            preset_text = re.sub(r'exclude_filter="[^"\n]*"', 'exclude_filter="*.gd,*.gdshader,*.gdshaderinc,*.gdextension,*.dll,*.so,*.dylib"', preset_text)
            presets.write_text(preset_text, encoding="utf-8")
            job = {"files": pack["files"], "preset": preset_name, "output": str(temporary)}
            write(stage / ".resource_export_job.json", job)
            run(args.godot, stage, log, "--export-pack", preset_name, str(temporary))
            if not temporary.is_file(): raise RuntimeError(f"No pack generated for {pack_id}; see {log}")
            sha = digest(temporary)
            relative = f"packs/{sha}.pck"
            destination = output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary.replace(destination)
            record = {"file": relative, "sha256": sha, "size": destination.stat().st_size}
            changed.append(pack_id)
        record["dependencies"] = pack["dependencies"]
        record["local"] = "res://resources/" + record["file"] if not pack["remote"] else ""
        packs[pack_id] = record
        states[pack_id] = {**record, "fingerprint": fingerprint}
    catalog = {"schema": 1, "platform": args.platform, "compatibility": compatibility, "remote_root": profile["remote_root"].rstrip("/") + "/" + args.platform,
               "entries": analysis["entries"], "packs": packs, "startup": sorted(key for key, pack in analysis["packs"].items() if pack["startup"])}
    version = hashlib.sha256(json.dumps(catalog, sort_keys=True).encode()).hexdigest()
    catalog["version"] = version
    relative_catalog = f"versions/{version}/catalog.json"
    write(output / relative_catalog, catalog)
    report = {"version": version, "rebuilt": changed, "reused": reused, "removed": sorted(set(previous.get("packs", {})) - set(packs)),
              "download_bytes": sum(packs[key]["size"] for key in changed), "cycles": analysis["cycles"], "packs": analysis["packs"]}
    old_entries = previous.get("entries", {})
    entries = {entry["address"]: entry for entry in analysis["entries"]}
    entry_fingerprints = {}
    for address, dependencies in analysis["entry_dependencies"].items():
        inputs = {}
        for path in dependencies:
            source = stage / path.removeprefix("res://")
            if source.is_file(): inputs[path] = digest(source)
            imported = source.with_name(source.name + ".import")
            if imported.is_file(): inputs[path + ".import"] = digest(imported)
        entry_fingerprints[address] = hashlib.sha256(json.dumps(inputs, sort_keys=True).encode()).hexdigest()
    report["addresses"] = {"added": sorted(set(entries) - set(old_entries)), "removed": sorted(set(old_entries) - set(entries)),
                           "modified": sorted(key for key in entries.keys() & old_entries.keys() if entries[key] != old_entries[key] or entry_fingerprints[key] != previous.get("entry_fingerprints", {}).get(key))}
    write(output / "report.json", report)
    write(output / "build_state.json", {"packs": states, "entries": entries, "entry_fingerprints": entry_fingerprints})
    # Install local build artifacts only after every content export succeeded.
    for record in packs.values():
        if record["local"]:
            target = project / record["local"].removeprefix("res://")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(output / record["file"], target)
    write(project / "resources/export_state.json", {"managed": sorted(path for pack in analysis["packs"].values() for path in pack["files"])})
    write(project / "resources/catalog.json", catalog)
    write(output / "latest.json", {"catalog": relative_catalog, "sha256": digest(output / relative_catalog)})
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--profile", default="")
    parser.add_argument("--platform", choices=("windows", "android", "ios"), default="windows")
    parser.add_argument("--analyze", action="store_true")
    args = parser.parse_args()
    (args.project.resolve() / ".godot/meowody_resources/cancel.json").unlink(missing_ok=True)
    result_path = args.project.resolve() / ".godot/meowody_resources/result.json"
    try:
        result = build(args)
        write(result_path, {"success": True, "result": result})
    except Exception as error:
        write(result_path, {"success": False, "error": str(error)})
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
