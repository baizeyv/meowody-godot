"""Source generation, version persistence and safe failure checks without downloads."""
import json
import hashlib
import io
import subprocess
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "meow/project_creator"))
from creator import generate_project, inspect_engine, download_engine, prepare_dependency, run, discover_engines, local_engine, release_versions


class ProjectCreatorTest(unittest.TestCase):
    def setUp(self):
        temporary_root = ROOT / "build/tests"
        temporary_root.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=temporary_root)
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.source = self.base / "source"
        for directory in ("src/runtime", "src/editor", "src/log", "src/reflection", "src/game", "src/ui/generated", "src/project_creator", "meow/project_creator",
                          "reflect_codegen", "tools/godot_mcp", ".agents/skills/meowody-framework", "meow/addons/meowody_binding", "meow/bin"):
            (self.source / directory).mkdir(parents=True)
        for filename in ("src/register_types.cpp", "src/register_types.h", "src/editor_registration.h", "src/editor_registration_stub.cpp",
                         "SConstruct", ".gitignore", ".clang-format", "README.md"):
            (self.source / filename).write_text("meow/", encoding="utf-8")
        (self.source / "framework-dependencies.json").write_text((ROOT / "framework-dependencies.json").read_text(), encoding="utf-8")
        (self.source / "src/game/business.cpp").touch()
        (self.source / "src/project_creator/project_creator_view.cpp").touch()
        (self.source / "meow/project_creator/creator.py").touch()
        (self.source / "src/ui/generated/OldView.cpp").touch()
        (self.source / "src/runtime/cache.obj").touch()
        (self.source / "meow/bin/meowody.gdextension").write_text('compatibility_minimum = "4.1"\nwindows="res://bin/native.dll"\n')
        (self.source / "meow/addons/meowody_binding/plugin.cfg").touch()
        (self.source / "meow/addons/meowody_resources").mkdir()
        (self.source / "meow/addons/meowody_resources/plugin.cfg").touch()
        (self.source / "meow/resources").mkdir()
        (self.source / "meow/resources/settings.json").write_text('{"schema":1,"entries":[]}', encoding="utf-8")
        self.destination = self.base / "中文 project"

    def generate(self):
        return generate_project(self.source, self.destination, '游戏 "Test"', "4.7.2.stable.official.abc123", "4.7", "a" * 40)

    def test_blank_framework_and_persisted_version(self):
        self.generate()
        self.assertTrue((self.destination / "project/main.tscn").is_file())
        self.assertEqual(json.loads((self.destination / "project/resources/settings.json").read_text()), {"schema": 1, "entries": []})
        self.assertIn('res://addons/meowody_resources/plugin.cfg', (self.destination / "project/project.godot").read_text())
        self.assertTrue((self.destination / "src/runtime").is_dir())
        self.assertFalse((self.destination / "src/game").exists())
        self.assertFalse((self.destination / "src/project_creator").exists())
        self.assertFalse((self.destination / "project/project_creator").exists())
        self.assertFalse((self.destination / "src/ui/generated/OldView.cpp").exists())
        self.assertFalse((self.destination / "src/runtime/cache.obj").exists())
        self.assertFalse((self.destination / "meow").exists())
        config = json.loads((self.destination / "meowody-project.json").read_text())
        self.assertEqual(config["project_dir"], "project")
        self.assertEqual(config["custom_api_file"], "godot-api/extension_api.json")
        self.assertIn('compatibility_minimum = "4.7"', (self.destination / "project/bin/meowody.gdextension").read_text())
        self.assertIn('config/name=' + json.dumps('游戏 "Test"', ensure_ascii=False),
                      (self.destination / "project/project.godot").read_text(encoding="utf-8"))

    def test_existing_directory_is_untouched(self):
        self.destination.mkdir()
        marker = self.destination / "keep.txt"
        marker.write_text("keep")
        with self.assertRaisesRegex(ValueError, "overwritten"):
            self.generate()
        self.assertEqual(marker.read_text(), "keep")

    def test_missing_pin_does_not_create_output(self):
        (self.source / "framework-dependencies.json").unlink()
        with self.assertRaises(FileNotFoundError):
            self.generate()
        self.assertFalse(self.destination.exists())

    def test_version_detection_and_rejection(self):
        with patch("creator.run", return_value="4.7.2.stable.official.abcdef"):
            self.assertEqual(inspect_engine(self.base / "Godot.exe")[1], "4.7")
        with patch("creator.run", return_value="3.5.stable.official.abcdef"):
            with self.assertRaises(ValueError):
                inspect_engine(self.base / "Godot.exe")
        with self.assertRaises(ValueError):
            download_engine("../../escape", self.base)

    def test_mismatched_submodule_fails_before_fetch(self):
        with patch("creator.run", return_value="160000 " + "a" * 40 + " 0\tgodot-cpp") as command:
            with self.assertRaisesRegex(ValueError, "differs"):
                prepare_dependency(self.source, self.destination, {"commit": "b" * 40}, print)
            self.assertEqual(command.call_count, 1)

    def test_real_submodule_registration_and_clone_restore(self):
        seed = self.base / "dependency-seed"
        run(["git", "init", seed], self.base, log=lambda _: None)
        (seed / "SConstruct").write_text("# fixture\n")
        run(["git", "add", "SConstruct"], seed, log=lambda _: None)
        run(["git", "-c", "user.name=Creator Test", "-c", "user.email=creator@example.invalid", "commit", "-m", "fixture"], seed, log=lambda _: None)
        commit = run(["git", "rev-parse", "HEAD"], seed, log=lambda _: None)
        run(["git", "init"], self.source, log=lambda _: None)
        self.generate()
        def local_checkout(_url, ref, destination, log):
            run(["git", "clone", "--no-hardlinks", seed, destination], destination.parent, log)
            run(["git", "checkout", "--detach", ref], destination, log)
        dependency = {"url": "https://github.com/godotengine/godot-cpp.git", "commit": commit}
        with patch("creator.checkout", side_effect=local_checkout):
            prepare_dependency(self.source, self.destination, dependency, log=lambda _: None)
        self.assertIn(dependency["url"], (self.destination / ".gitmodules").read_text())
        gitlink = run(["git", "ls-files", "--stage", "godot-cpp"], self.destination, log=lambda _: None)
        self.assertTrue(gitlink.startswith("160000 " + commit))
        self.assertTrue((self.destination / "godot-cpp/.git").is_file())
        result = subprocess.run(["git", "rev-parse", "--verify", "HEAD"], cwd=self.destination, capture_output=True)
        self.assertNotEqual(result.returncode, 0)  # Creator made no parent commit.
        run(["git", "-c", "user.name=Creator Test", "-c", "user.email=creator@example.invalid", "commit", "-m", "fixture project"], self.destination, log=lambda _: None)
        restored = self.base / "restored"
        run(["git", "clone", "--no-hardlinks", self.destination, restored], self.base, log=lambda _: None)
        run(["git", "config", "submodule.godot-cpp.url", seed], restored, log=lambda _: None)
        run(["git", "-c", "protocol.file.allow=always", "submodule", "update", "--init", "--recursive"], restored, log=lambda _: None)
        self.assertEqual(run(["git", "rev-parse", "HEAD"], restored / "godot-cpp", log=lambda _: None), commit)

    def test_download_checksum_and_cache(self):
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, "w") as package:
            package.writestr("Godot_v4.7.2-stable_win64.exe", b"test editor")
        payload = archive.getvalue()
        checksum = hashlib.sha512(payload).hexdigest()
        responses = [io.BytesIO(payload), io.BytesIO((checksum + "  Godot_v4.7.2-stable_win64.exe.zip\n").encode())]
        with patch("creator.urllib.request.urlopen", side_effect=responses) as request:
            executable = download_engine("4.7.2-stable", self.base, log=lambda _: None)
            self.assertEqual(executable.read_bytes(), b"test editor")
            self.assertEqual(download_engine("4.7.2-stable", self.base), executable)
            self.assertEqual(request.call_count, 2)

    def test_bad_checksum_never_marks_engine_ready(self):
        responses = [io.BytesIO(b"bad archive"), io.BytesIO(("0" * 128 + "  Godot_v4.7.2-stable_win64.exe.zip\n").encode())]
        with patch("creator.urllib.request.urlopen", side_effect=responses):
            with self.assertRaisesRegex(RuntimeError, "checksum"):
                download_engine("4.7.2-stable", self.base, log=lambda _: None)
        self.assertFalse((self.base / "engines/4.7.2-stable/ready.json").exists())

    def test_native_helper_protocol_reports_errors_without_touching_destination(self):
        request, response = self.base / "request.json", self.base / "response.json"
        request.write_text(json.dumps({"operation": "unknown"}), encoding="utf-8")
        result = subprocess.run([sys.executable, "-X", "utf8", str(ROOT / "meow/project_creator/creator.py"),
                                 str(request), str(response)], capture_output=True)
        self.assertEqual(result.returncode, 1)
        payload = json.loads(response.read_text(encoding="utf-8"))
        self.assertFalse(payload["ok"])
        self.assertIn("Unknown", payload["message"])
        self.assertFalse(self.destination.exists())

    def test_engine_discovery_filters_architecture_versions_and_duplicates(self):
        def executable(name, machine=b"\x64\x86"):
            path = self.base / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"MZ" + bytes(58) + (64).to_bytes(4, "little") + b"PE\0\0" + machine)
            return path
        good = executable("Godot official/Godot_v4.7.2-stable_win64.exe")
        older = executable("Godot_v4.6-stable_win64.exe")
        executable("Godot_v4.7.2-stable_win64_console.exe")
        executable("Godot_arm64.exe", b"\x64\xaa")
        executable("Godot_dev.exe")
        executable("Godot_hung.exe")
        executable("Godot_template.exe")
        executable("Godot_invalid.exe").write_bytes(b"not an executable")
        def inspect(command, **kwargs):
            filename = Path(command[0]).stem
            if "hung" in filename:
                raise subprocess.TimeoutExpired(command, 5)
            if command[1] == "--help":
                return subprocess.CompletedProcess(command, 0, "export template" if "template" in filename else "--editor --dump-extension-api")
            value = "4.7.2.dev.official.abcdef" if "dev" in filename else "4.6.0.stable.official.abcdef" if "4.6" in filename else "4.7.2.stable.official.abcdef"
            return subprocess.CompletedProcess(command, 0, value)
        with patch("creator.subprocess.run", side_effect=inspect), patch("creator.os.get_exec_path", return_value=[]):
            found = discover_engines(self.base, seeds=[good, str(good).upper()], roots=[self.base], log=lambda _: None)
            self.assertEqual([item["path"] for item in found], [str(good.resolve()), str(older.resolve())])
        with self.assertRaises(ValueError):
            local_engine(self.base / "Godot_arm64.exe")

    def test_online_versions_select_latest_stable_and_exclude_previews(self):
        latest = {"tag_name": "4.7.2-stable"}
        releases = [latest, {"tag_name": "4.8-dev1", "prerelease": True}, {"tag_name": "4.6-stable"},
                    {"tag_name": "4.7.3-stable", "draft": True}, {"tag_name": "3.6-stable"}]
        responses = [io.BytesIO(json.dumps(value).encode()) for value in (latest, releases)]
        with patch("creator.urllib.request.urlopen", side_effect=responses) as request:
            self.assertEqual(release_versions(), ["4.7.2-stable", "4.6-stable"])
            self.assertTrue(request.call_args_list[0].args[0].full_url.endswith("/releases/latest"))


if __name__ == "__main__":
    unittest.main()
