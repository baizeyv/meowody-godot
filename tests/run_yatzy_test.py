"""Run deterministic game tests without starting Godot: python tests/run_yatzy_test.py."""
from pathlib import Path
from godot_test_workspace import prepare_workspace
import argparse
import os
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--godot", type=Path, help="Also test the built extension in an isolated Godot project")
parser.add_argument("--visual", action="store_true", help="Render the UI and save tests/workspace/meow/ui.png")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
output = root / "build" / "tests"
output.mkdir(parents=True, exist_ok=True)
executable = output / "yatzy_test.exe"
subprocess.run([
    "clang++", "-std=c++20", "-DYATZY_TEST", "-I" + str(root / "src"),
    str(root / "src/game/yatzy_rules.cpp"), str(root / "tests/game/yatzy_test.cpp"),
    "-o", str(executable),
], check=True)
subprocess.run([str(executable)], check=True)

if args.godot:
    project = prepare_workspace()
    shutil.copy2(root / "meow/bin/libmeowody.windows.template_debug.x86_64.dll", project / "yatzy.dll")
    (project / "project.godot").write_text('config_version=5\n[application]\nconfig/name="Yahtzee test"\n', encoding="utf-8")
    (project / "yatzy.gdextension").write_text(
        '[configuration]\nentry_symbol="meowody_library_init"\ncompatibility_minimum="4.7"\n'
        '[libraries]\nwindows="res://yatzy.dll"\n', encoding="utf-8")
    (project / ".godot").mkdir(exist_ok=True)
    (project / ".godot/extension_list.cfg").write_text("res://yatzy.gdextension\n", encoding="utf-8")
    (project / "test.gd").write_text('''extends SceneTree
var changes = 0
func check(value):
    if not value:
        push_error("Yahtzee integration check failed")
        quit(1)
        return false
    return true
func _initialize():
    call_deferred("run_tests")
func run_tests():
    var game = ClassDB.instantiate("YatzyGame")
    game.state_changed.connect(func(): changes += 1)
    if not check(game.get_category_names().size() == 13): return
    if not check(game.get_state().rolls_left == 3 and not game.score(12)): return
    if not check(game.roll() and game.set_held(0, true)): return
    var held_value = game.get_state().dice[0]
    if not check(game.roll() and game.get_state().dice[0] == held_value): return
    if not check(game.roll() and not game.roll()): return
    var expected = game.preview_score(12)
    var snapshot = game.get_state()
    snapshot.scores[12] = 999
    if not check(game.get_state().scores[12] == -1): return
    if not check(game.score(12) and game.get_state().total == expected): return
    for category in range(12):
        if not check(game.roll() and game.score(category)): return
    if not check(game.get_state().finished and not game.roll()): return
    if not check(changes == 29): return
    game.reset()
    if not check(game.get_state().total == 0 and changes == 30): return
    game = null
    print("Yahtzee Godot: PASS")
    quit(0)
''', encoding="utf-8")
    environment = os.environ.copy()
    for folder in ("profile", "temp"):
        (output / folder).mkdir(exist_ok=True)
    environment["APPDATA"] = environment["LOCALAPPDATA"] = str(output / "profile")
    environment["TEMP"] = environment["TMP"] = str(output / "temp")
    result = subprocess.run([str(args.godot.resolve()), "--headless", "--path", str(project),
        "--script", "res://test.gd"], env=environment, capture_output=True, text=True, timeout=60)
    log = result.stdout + result.stderr
    (output / "godot.log").write_text(log, encoding="utf-8")
    if result.returncode or "Yahtzee Godot: PASS" not in log or "SCRIPT ERROR:" in log:
        raise RuntimeError(log)
    print("Yahtzee Godot: PASS")
    shutil.copy2(root / "meow/game/yatzy.tscn", project / "yatzy.tscn")
    shutil.copy2(root / "tests/game/ui_test.gd", project / "ui_test.gd")
    (project / "project.godot").write_text(
        'config_version=5\n[application]\nconfig/name="Yahtzee UI test"\n'
        '[display]\nwindow/size/viewport_width=960\nwindow/size/viewport_height=900\n'
        '[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding="utf-8")
    command = [str(args.godot.resolve()), "--path", str(project), "--script", "res://ui_test.gd"]
    command += ["--", "--visual"] if args.visual else ["--headless"]
    result = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=60)
    log = result.stdout + result.stderr
    (output / "ui.log").write_text(log, encoding="utf-8")
    if result.returncode or "Yahtzee UI: PASS" not in log or "SCRIPT ERROR:" in log or "ERROR:" in log:
        raise RuntimeError(log)
    print("Yahtzee UI: PASS")
