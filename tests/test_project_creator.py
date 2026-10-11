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
sys.path.insert(0, str(ROOT / "tools/project_creator"))
from creator import generate_project, inspect_engine, download_engine, prepare_dependency, run


class ProjectCreatorTest(unittest.TestCase):
    def setUp(self):
        temporary_root = ROOT / "build/tests"
        temporary_root.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=temporary_root)
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.source = self.base / "source"
        for directory in ("src/runtime", "src/editor", "src/log", "src/reflection", "src/game", "src/ui/generated",
                          "reflect_codegen", "tools/godot_mcp", ".agents/skills/meowody-framework", "meow/addons/meowody_binding", "meow/bin"):
            (self.source / directory).mkdir(parents=True)
        for filename in ("src/register_types.cpp", "src/register_types.h", "src/editor_registration.h", "src/editor_registration_stub.cpp",
                         "SConstruct", ".gitignore", ".clang-format", "README.md"):
            (self.source / filename).write_text("meow/", encoding="utf-8")
        (self.source / "framework-dependencies.json").write_text((ROOT / "framework-dependencies.json").read_text(), encoding="utf-8")
        (self.source / "src/game/business.cpp").touch()
        (self.source / "src/ui/generated/OldView.cpp").touch()
        (self.source / "src/runtime/cache.obj").touch()
        (self.source / "meow/bin/meowody.gdextension").write_text('compatibility_minimum = "4.1"\nwindows="res://bin/native.dll"\n')
        (self.source / "meow/addons/meowody_binding/plugin.cfg").touch()
        self.destination = self.base / "中文 project"

    def generate(self):
        return generate_project(self.source, self.destination, '游戏 "Test"', "4.7.2.stable.official.abc123", "4.7", "a" * 40)

    def test_blank_framework_and_persisted_version(self):
        self.generate()
        self.assertTrue((self.destination / "project/main.tscn").is_file())
        self.assertTrue((self.destination / "src/runtime").is_dir())
        self.assertFalse((self.destination / "src/game").exists())
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


if __name__ == "__main__":
    unittest.main()
