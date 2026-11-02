#!/usr/bin/env python3
"""Build real i386 wrapper fixtures and installed-library API controls; never boot."""
import argparse
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
lane = Path(__file__).resolve().parent
shared_lane = lane.parent / 'i386-guest'
source = shared_lane / 'superproject'
xnu = source / 'src/external/xnu'
steps = json.loads((shared_lane / 'build/steps.json').read_text())
cwd = lane.parent / 'stage41d/build'
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=True)

def pick(prefix):
    return next(v[0][:] for k, v in steps.items() if k.startswith(prefix))

def compile_cmd(template, source, target):
    cmd = template[:]
    cmd[cmd.index('-c') + 1] = str(source)
    cmd[cmd.index('-o') + 1] = str(target)
    return cmd

c_template = pick('hello-compile-i386-')
link_template = pick('hello-link-i386-')
asm_template = next(v[0][:] for v in steps.values()
    if v[0][0].endswith('/clang') and '-c' in v[0]
    and v[0][v[0].index('-c') + 1].endswith('/___lseek.S'))
for i in range(len(asm_template) - 2, -1, -1):
    if asm_template[i] == '-arch':
        del asm_template[i:i+2]
asm_template += ['-arch', 'i386', '-DSYS_intprobe=SYS_thread_selfid']

def run(cmd):
    subprocess.run(cmd, cwd=cwd, check=True)

def link(objects, binary):
    cmd = link_template[:]
    i = cmd.index(str(shared_lane / 'build/hello-i386.o'))
    cmd[i:i+1] = list(map(str, objects))
    cmd[cmd.index('-o') + 1] = str(binary)
    cmd += ['-Wl,-pagezero_size,0x10000']
    run(cmd)

for variant, ref in [('before', 'e912fb4'), ('after', None)]:
    work = out / variant
    cmd = ['python3', str(xnu / 'darling/tests/i386-wrapper-paths.py'), '--emit-guest', '--out', str(work)]
    if ref:
        cmd += ['--ref', ref]
    run(cmd)
    aliases = json.loads((work / 'rename.json').read_text())
    c_obj = work / 'check.o'
    run(compile_cmd(c_template, work / 'check.c', c_obj))
    fixture_obj = work / 'fixture.o'
    run(compile_cmd(c_template, work / 'fixture.S', fixture_obj))
    for tag in ['custom', 'generated']:
        objects = [c_obj, fixture_obj]
        for asm in sorted((work / tag).glob('*.S')):
            target = asm.with_suffix('.o')
            cmd = compile_cmd(asm_template, asm, target)
            cmd += ['-D' + name + '=' + value for name, value in aliases.items() if name != '__current_pid']
            run(cmd)
            objects.append(target)
        link(objects, out / (variant + '-' + tag + '-i386'))

for arch in ['i386', 'x86_64']:
    obj = out / ('installed-' + arch + '.o')
    run(compile_cmd(pick('hello-compile-' + arch + '-'), lane / 'installed-errors.c', obj))
    cmd = pick('hello-link-' + arch + '-')
    cmd[cmd.index(str(shared_lane / ('build/hello-' + arch + '.o')))] = str(obj)
    cmd[cmd.index('-o') + 1] = str(out / ('installed-' + arch))
    if arch == 'i386':
        cmd += ['-Wl,-pagezero_size,0x10000']
    run(cmd)
run(['python3', str(lane / 'check-fixture-maps.py'), '--out', str(out),
     *map(str, sorted(out.glob('*-i386')))])
print('Built before/after custom/generated i386 fixtures and both installed-API controls:', out)
