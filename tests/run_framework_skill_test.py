"""Validate portable skill links and compile/run the documented examples."""
import argparse
import os
from pathlib import Path
import subprocess
from framework_skill_checks import extract_examples, validate_skill
from godot_test_workspace import prepare_workspace


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--godot', type=Path)
    parser.add_argument('--project', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    validate_skill(args.project.resolve() if args.project else root)
    if not args.godot:
        return
    scratch = prepare_workspace()
    # These other suites are not part of this probe and have unavailable plugin preloads.
    (scratch / 'tests/.gdignore').touch()
    count = extract_examples(root, scratch)
    env = os.environ.copy()
    env['MEOWODY_SKILL_OUTPUT'] = str(scratch)
    compile_env = env.copy()
    for folder, keys in (('profile', ('APPDATA', 'LOCALAPPDATA')), ('temp', ('TEMP', 'TMP'))):
        (scratch / folder).mkdir()
        (scratch / folder / '.gdignore').touch()
        for key in keys:
            env[key] = str(scratch / folder)
    with (scratch / 'compile.log').open('w', encoding='utf-8') as log:
        compiled = subprocess.run(['scons', '-f', 'tests/SConstruct.framework_skill'], cwd=root, env=compile_env,
                                  stdout=log, stderr=subprocess.STDOUT)
    if compiled.returncode:
        raise RuntimeError((scratch / 'compile.log').read_text(encoding='utf-8', errors='replace'))
    (scratch / 'test.gdextension').write_text('[configuration]\nentry_symbol="framework_skill_test_init"\ncompatibility_minimum="4.7"\n[libraries]\nwindows="res://framework_skill_test.dll"\n', encoding='utf-8')
    (scratch / '.godot').mkdir()
    (scratch / '.godot/extension_list.cfg').write_text('res://test.gdextension\n', encoding='utf-8')
    (scratch / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Framework skill examples"\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding='utf-8')
    (scratch / 'probe.gd').write_text('''extends SceneTree
func _initialize():
    call_deferred("run")
func run():
    var probe = ClassDB.instantiate("FrameworkSkillProbe")
    var okay = probe.run()
    probe = null
    var panel = load("res://owned_panel.gd").new()
    var undo_id = panel.undo.get_instance_id()
    panel.free()
    okay = okay and not is_instance_id_valid(undo_id)
    print("[FrameworkSkill] PASS documented GDScript panel releases UndoRedo" if okay else "[FrameworkSkill] FAIL panel/probe")
    quit(0 if okay else 1)
''', encoding='utf-8')
    result = subprocess.run([str(args.godot.resolve()), '--headless', '--verbose', '--path', str(scratch),
                             '--script', 'res://probe.gd'], env=env, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=45)
    output = result.stdout + result.stderr
    (scratch / 'runtime.log').write_text(output, encoding='utf-8')
    report = root / 'build/test-results/framework-skill.log'
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(output, encoding='utf-8')
    errors = [line for line in output.splitlines() if line.startswith(('ERROR:', 'SCRIPT ERROR:')) and 'root certificate store' not in line]
    if result.returncode or errors or 'ObjectDB instance' in output or '[FrameworkSkill] FAIL' in output or 'PASS documented GDScript' not in output:
        raise RuntimeError(output[-12000:])
    print('\n'.join(line for line in output.splitlines() if '[FrameworkSkill]' in line))
    print(f'[FrameworkSkill] PASS {count} compiled C++ examples; runtime cleanup verified')


if __name__ == '__main__':
    main()
