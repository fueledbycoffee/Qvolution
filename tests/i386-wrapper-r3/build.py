#!/usr/bin/env python3
"""Replay only the inherited mldr build edges in this lane's isolated worktree."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

lane = Path(__file__).resolve().parent
source, build = lane / 'source', lane / 'build'
shared = lane.parent / 'i386-guest'
template = shared / 'build/steps.json'
cwd = lane.parent / 'stage41d/build'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def sources():
    paths = subprocess.check_output(['git', '-C', str(source), 'ls-files', '--', 'src/startup/mldr'], text=True).splitlines()
    return [source / p for p in paths if Path(p).suffix in ('.c', '.h')
            and not any(part.startswith('TAINTED') or part in ('tainted', 'CryptoTokenKit') for part in Path(p).parts)
            and not (source / p).is_symlink()]

def provenance():
    assert not subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip()
    return {'commit': subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip(),
            'template_sha256': digest(template), 'steps_sha256': digest(build / 'steps.json'),
            'images': {name: digest(build / name) for name in ('mldr', 'mldr32')},
            'sources': {str(p): digest(p) for p in sources()}}

action = sys.argv[1]
if action == 'step':
    subprocess.run(json.loads((build / 'steps.json').read_text())[sys.argv[2]], cwd=cwd, check=True)
elif action == 'verify':
    assert provenance() == json.loads((build / 'provenance.json').read_text())
    dry = subprocess.check_output(['ninja', '-C', str(build), '-n'], text=True)
    assert dry.rstrip().endswith('ninja: no work to do.'), dry
    print('PASS: committed isolated loader source, binaries and idle build verified')
elif action == 'build':
    assert not subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip()
    build.mkdir(exist_ok=True)
    steps, edges = {}, []
    dependencies = sources()
    for key, commands in json.loads(template.read_text()).items():
        command = commands[0]
        compile_loader = '-c' in command and '/src/startup/mldr/' in command[command.index('-o') + 1]
        if not compile_loader and not key.startswith(('mldr-', 'mldr32-')):
            continue
        command = [arg.replace(str(shared / 'build'), str(build))
                   .replace(str(shared / 'superproject/src/startup/mldr'), str(source / 'src/startup/mldr'))
                   .replace(str(lane.parent / 'stage41d/superproject/src/startup/mldr'), str(source / 'src/startup/mldr'))
                   for arg in command]
        # The inherited xnu overlay is a read-only dependency, not a generated lane input.
        command = [arg.replace(str(build / 'include'), str(shared / 'build/include')) for arg in command]
        if compile_loader:
            # Vendored include symlinks resolve through the populated inherited SDK/xnu.
            command.insert(1, '-I' + str(lane.parent / 'stage41d/superproject/src/startup/mldr/include'))
        output = Path(command[command.index('-o') + 1])
        output.parent.mkdir(parents=True, exist_ok=True)
        steps[key] = command
        deps = [Path(command[command.index('-c') + 1]), *dependencies] if compile_loader else [Path(arg) for arg in command if arg.endswith('.o')]
        edges.append(f'build {output}: step ' + ' '.join(map(str, deps)) + f'\n  key = {key}\n')
    assert len(steps) == 12, len(steps)
    (build / 'steps.json').write_text(json.dumps(steps, indent=2) + '\n')
    (build / 'build.ninja').write_text(f'rule step\n  command = python3 {lane}/build.py step $key\n' + '\n'.join(edges))
    subprocess.run(['ninja', '-C', str(build), '-j6'], check=True)
    dry = subprocess.check_output(['ninja', '-C', str(build), '-n'], text=True)
    assert dry.rstrip().endswith('ninja: no work to do.'), dry
    print(dry, end='')
    (build / 'provenance.json').write_text(json.dumps(provenance(), indent=2) + '\n')
else:
    raise SystemExit('usage: build.py build|verify|step KEY')
