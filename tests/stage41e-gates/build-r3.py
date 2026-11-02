#!/usr/bin/env python3
"""Reuse Stage41e's full fat link recipes without writing to its build tree."""
import json
from pathlib import Path
import shlex
import subprocess
import sys

L = Path(__file__).resolve().parents[3]
S = L / 'superproject'
BASE = L.parent / 'stage41e'
B = BASE / 'build'
OUT = L / 'build-r3'
CHANGED = {'Automator': ['AMAction', 'AMWorkflow'],
           'AVFAudio': ['AVAudioPlayerNode'], 'MediaPlayer': ['MediaPlayer']}


def recipe(rule):
    rows = json.loads(subprocess.check_output(['ninja', '-C', str(B), '-t', 'compdb', '-x', rule], text=True))
    return rows


def execute(command, log):
    log.append(command)
    subprocess.run(command, cwd=B, check=True)


def framework(name):
    substitutions = {}
    log = []
    rows = recipe(f'C_COMPILER__{name}_unscanned_')
    for stem in CHANGED[name]:
        target = f'src/frameworks/{name}/CMakeFiles/{name}.dir/src/{stem}.m.o'
        row, = [r for r in rows if r['output'] == target]
        command = shlex.split(row['command'])
        for flag in ('-o', '-MF', '-MT'):
            index = command.index(flag) + 1
            command[index] = str(OUT / Path(command[index]).name)
        source = str(S / f'src/frameworks/{name}/src/{stem}.m')
        command[-1] = source
        # Quoted includes resolve beside the changed source; framework headers
        # and unchanged payload dependencies remain Stage41e's reviewed inputs.
        execute(command, log)
        substitutions[target] = str(OUT / Path(target).name)
    row, = recipe(f'C_SHARED_LIBRARY_LINKER__{name}_')
    command = shlex.split(row['command'])
    assert command[:2] == [':', '&&'] and command[-2:] == ['&&', ':']
    command = command[2:-2]
    assert not any(x in command for x in ('&&', ';', '|'))
    command = [substitutions.get(x, x) for x in command]
    command[command.index('-o') + 1] = str(OUT / name)
    execute(command, log)
    (OUT / (name + '-commands.json')).write_text(json.dumps(log, indent=2) + '\n')
    # Only OUR linked payload is inspected. Never point this at Apple images.
    lipo = B / 'src/external/cctools-port/cctools/misc/lipo'
    info = subprocess.check_output([str(lipo), '-info', str(OUT / name)], text=True)
    assert 'i386' in info and 'x86_64' in info, info
    print(info, end='', flush=True)


def control():
    delta = subprocess.check_output(['git', '-C', str(S), 'diff', 'd0872f92427^',
                                    'd0872f92427', '--', 'src/startup/darling.c'])
    result = subprocess.run(['patch', '--reverse', '--batch', '--fuzz=0', '--output',
                             str(OUT / 'darling-inherited.c'), str(S / 'src/startup/darling.c')],
                            input=delta, check=True)
    assert result.returncode == 0


def guest():
    row = json.loads((BASE / 'fixtures/stage41e/specific/receipts.json').read_text())['stub-forwarding']
    substitutions = {row['source']: str(S / 'tests/stub-forwarding/guest.m'),
                     row['binary']: str(OUT / 'stub-forwarding'),
                     str(BASE / 'fixtures/stage41e/specific/stub-forwarding.o'): str(OUT / 'stub-forwarding.o')}
    log = []
    for step in ('compile', 'link'):
        execute([substitutions.get(x, x) for x in row[step]], log)
    (OUT / 'guest-commands.json').write_text(json.dumps(log, indent=2) + '\n')


if __name__ == '__main__':
    OUT.mkdir(exist_ok=True)
    name, = sys.argv[1:]
    if name == 'control':
        control()
    elif name == 'guest':
        guest()
    else:
        framework(name)
