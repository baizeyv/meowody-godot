"""Check the skill as distributed in either a framework or generated project."""
import json
from pathlib import Path
import re


SKILL_PATH = Path('.agents/skills/meowody-framework')


def validate_skill(root, expected_source=None):
    root = Path(root)
    metadata_path = root / 'meowody-project.json'
    metadata = json.loads(metadata_path.read_text(encoding='utf-8')) if metadata_path.exists() else {}
    project = root / metadata.get('project_dir', 'meow')
    handbook = root / ('FRAMEWORK.md' if (root / 'FRAMEWORK.md').exists() else 'README.md')
    assert (root / 'src/runtime/app.h').is_file() and (root / 'SConstruct').is_file()
    assert (project / 'project.godot').is_file() and handbook.is_file()
    skill = root / SKILL_PATH
    assert (skill / 'SKILL.md').is_file() and (skill / 'agents/openai.yaml').is_file()
    files = {p.relative_to(skill): p for p in skill.rglob('*') if p.is_file()}
    if expected_source is not None:
        source = Path(expected_source) / SKILL_PATH
        originals = {p.relative_to(source): p for p in source.rglob('*') if p.is_file()}
        assert files.keys() == originals.keys(), 'Skill distribution omitted or introduced files'
        for name in originals:
            assert files[name].read_bytes() == originals[name].read_bytes(), f'Skill was rewritten: {name}'
        assert not (root / '.codex').exists(), 'Machine-specific configuration was distributed'
        assert not (root / 'tests').exists(), 'Framework test artifacts were distributed'

    anchors = set(re.findall(r'<a id="([^"]+)"', handbook.read_text(encoding='utf-8')))
    reachable = {skill / 'SKILL.md'}
    links = {}
    for path in files.values():
        assert path.suffix not in {'.dll', '.obj', '.pdb', '.exe', '.pyc'}, f'Build artifact in skill: {path}'
        if path.suffix != '.md':
            continue
        body = path.read_text(encoding='utf-8')
        prose = re.sub(r'```.*?```', '', body, flags=re.S)
        targets = []
        for link in re.findall(r'\[[^\]]+\]\(([^)]+)\)', prose):
            if '://' in link or link.startswith('#'):
                continue
            target = (path.parent / link.split('#', 1)[0]).resolve()
            assert target.is_relative_to(skill.resolve()) and target.is_file(), f'Broken skill link: {path}: {link}'
            targets.append(target)
        links[path.resolve()] = targets
        for anchor in re.findall(r'(?<![\w])#([a-z][a-z0-9-]+)(?![\w*-])', prose):
            if anchor in {'anchor', 'node'}:
                continue
            assert anchor in anchors, f'Unknown handbook anchor: {anchor} in {path}'
        # Concrete source paths are checked against the project layout, not this machine.
        for code in re.findall(r'`([^`\n]+)`', body):
            code = code.split('::', 1)[0]
            if ' ' in code or '*' in code or code.endswith('/'):
                continue
            if code.startswith(('src/', 'reflect_codegen/', 'addons/', 'tools/', 'tests/')):
                if code.startswith('tests/') and not (root / 'tests').exists():
                    continue  # Framework-only drivers explicitly require existence checks.
                if code.startswith('src/project_creator/') and not (root / 'src/project_creator').exists():
                    continue
                base = project if code.startswith('addons/') else root
                assert (base / code).exists(), f'Missing documented source: {code}'
    pending = [p.resolve() for p in reachable]
    visited = set()
    while pending:
        path = pending.pop()
        if path in visited:
            continue
        visited.add(path)
        pending.extend(links.get(path, []))
    assert all(p.resolve() in visited for p in files.values() if p.suffix == '.md'), 'Unreachable topic guide'
    print(f'[FrameworkSkill] PASS {len(files)} distributed files, links, anchors and {project.name}/ layout')


def extract_examples(root, output):
    document = (Path(root) / SKILL_PATH / 'references/examples.md').read_text(encoding='utf-8')
    cpp = re.findall(r'```cpp\n(.*?)```', document, re.S)
    gdscript = re.findall(r'```gdscript\n(.*?)```', document, re.S)
    assert cpp and gdscript, 'Missing executable examples'
    (output / 'skill_examples.h').write_text('#pragma once\n' + '\n'.join(cpp), encoding='utf-8')
    assert len(gdscript) == 1
    (output / 'owned_panel.gd').write_text(gdscript[0], encoding='utf-8')
    return len(cpp)
