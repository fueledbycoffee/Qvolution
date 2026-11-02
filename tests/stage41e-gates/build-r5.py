#!/usr/bin/env python3
"""Relink committed fatal diagnostics against Stage41e; never write its build."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import uuid

L = Path(__file__).resolve().parents[3]
B = L.parent / 'stage41e/build'
P = L.parent / 'preview-r10b'
OUT = L / 'build-r5'
HEADS = {'libc': '28ce039436b9b0fa43d99b89bff0fc4230dfea72',
         'xnu': '63ef64bba94f184abb0d3613f08a7a070e3316ee'}


def sha(p):
    with Path(p).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def recipe(rule):
    return json.loads(subprocess.check_output(
        ['ninja', '-C', str(B), '-t', 'compdb', '-x', rule], text=True))


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--step':
        step = json.loads(Path(sys.argv[2]).read_text())
        for command in step:
            if command[0].endswith('-ar'):
                archive = Path(command[2])
                if archive.exists():
                    archive.rename(archive.with_name(archive.name+'.prior-'+uuid.uuid4().hex[:8]))
            subprocess.run(command, cwd=B, check=True)
            if command[0].endswith('-ranlib'):
                # cctools ranlib truncates the archive mtime to whole seconds.
                os.utime(command[1], None)
        return
    assert len(sys.argv) == 1
    OUT.mkdir(exist_ok=True)
    for component, head in HEADS.items():
        assert subprocess.check_output(['git', '-C', str(P/component), 'rev-parse', 'HEAD'], text=True).strip() == head
        assert not subprocess.check_output(['git', '-C', str(P/component), 'status', '--porcelain'])
    headers = [P/'libc/include'/name for name in ('QVFatal.h', 'QVDiagnostic.h', 'QVProgress.h')]
    headers.append(Path(__file__).with_name('fatal-include')/'QVFatal.h')
    replacements = {}
    nodes = []
    inputs = {str(p): sha(p) for p in headers}

    def node(output, commands, dependencies):
        path = OUT/(Path(output).name + '.commands.json')
        path.write_text(json.dumps(commands, indent=2)+'\n')
        nodes.append((str(output), path, [str(Path(__file__)), *map(str, dependencies)]))

    for rule, component, endings in (
            ('C_COMPILER__libc-stdlib_unscanned_', 'libc', ('/abort.c',)),
            ('C_COMPILER__libc-stdlib_dyld_unscanned_', 'libc', ('/abort.c',)),
            ('C_COMPILER__emulation_unscanned_', 'xnu', ('/simple.c', '/abort_with_payload.c')),
            ('C_COMPILER__emulation_dyld_unscanned_', 'xnu', ('/simple.c', '/abort_with_payload.c'))):
        for row in recipe(rule):
            if not row['file'].endswith(endings):
                continue
            relative = row['file'].split('/src/external/'+component+'/', 1)[1]
            source = P/component/relative
            assert source.is_file(), source
            output = OUT/(rule.split('__')[1]+'-'+Path(source).name+'.o')
            command = shlex.split(row['command'])
            command[1:1] = ['-I', str(Path(__file__).with_name('fatal-include')), '-idirafter', str(P/'libc/include')]
            command[-1] = str(source)
            for flag in ('-o', '-MF', '-MT'):
                command[command.index(flag)+1] = str(output) + ('.d' if flag == '-MF' else '')
            inputs[str(source)] = sha(source)
            replacements[row['output']] = str(output)
            node(output, [command], [source, *headers])

    payloads = {}
    for rule in ('C_SHARED_LIBRARY_LINKER__system_c_',
                 'C_SHARED_LIBRARY_LINKER__system_kernel_',
                 'C_STATIC_LIBRARY_LINKER__libc_static_',
                 'ASM_STATIC_LIBRARY_LINKER__libc_static32_',
                 'ASM_STATIC_LIBRARY_LINKER__libc_static64_',
                 'C_STATIC_LIBRARY_LINKER__system_kernel_static32_',
                 'C_STATIC_LIBRARY_LINKER__system_kernel_static64_',
                 'CXX_EXECUTABLE_LINKER__system_loader_'):
        row, = recipe(rule)
        output = OUT/Path(row['output']).name
        if output.suffix == '.a':
            output = output.with_name(output.stem+'-v2.a')
        args = shlex.split(row['command'])
        assert args[:2] == [':', '&&'] and args[-2:] == ['&&', ':']
        args = args[2:-2]
        steps, part = [], []
        for arg in [*args, '&&']:
            if arg != '&&':
                assert arg not in (';', '|')
                part.append(arg)
                continue
            if part[:4] == ['/usr/bin/cmake', '-E', 'rm', '-f']:
                # Fresh private archive: never execute baseline's rm step.
                assert part[4:] == [row['output']]
            else:
                steps.append(part)
            part = []
        dependencies = []
        for step in steps:
            for arg in step:
                if arg in replacements:
                    dependencies.append(replacements[arg])
                elif arg.endswith(('.o', '.a', '.dylib')) and (B/arg).is_file():
                    inputs[str(B/arg)] = sha(B/arg)
            for i, arg in enumerate(step):
                step[i] = str(output) if arg == row['output'] else replacements.get(arg, arg)
        node(output, steps, dependencies)
        replacements[row['output']] = str(output)
        payloads[Path(output).name] = str(output)
    probe_commands = json.loads((L/'build-r3/guest-commands.json').read_text())
    substitutions = {
        str(L/'superproject/tests/stub-forwarding/guest.m'): str(Path(__file__).with_name('fatal-probe.c')),
        str(L/'build-r3/stub-forwarding.o'): str(OUT/'fatal-probe.o'),
        str(L/'build-r3/stub-forwarding'): str(OUT/'fatal-probe'),
    }
    commands = [[substitutions.get(arg, arg) for arg in step] for step in probe_commands]
    commands[0][1:1] = ['-x', 'objective-c']
    node(OUT/'fatal-probe', commands, [Path(__file__).with_name('fatal-probe.c')])
    inputs[str(Path(__file__).with_name('fatal-probe.c'))] = sha(Path(__file__).with_name('fatal-probe.c'))
    payloads['fatal-probe'] = str(OUT/'fatal-probe')
    ninja = ['rule step', '  command = python3 '+shlex.quote(str(Path(__file__)))+' --step $commands',
             '  description = build $out']
    for output, command_file, deps in nodes:
        ninja += ['build '+output+': step '+' '.join([str(command_file), *deps]),
                  '  commands = '+str(command_file)]
    ninja += ['default '+' '.join(payloads.values())]
    (OUT/'build.ninja').write_text('\n'.join(ninja)+'\n')
    (OUT/'build-plan.json').write_text(json.dumps(dict(heads=HEADS, inputs=inputs,
        commands={str(p):sha(p) for _,p,_ in nodes}, payloads=payloads), indent=2)+'\n')
    print('PASS generated',len(nodes),'compile/link nodes;',len(inputs),'hashed source/link inputs')


if __name__ == '__main__':
    main()
