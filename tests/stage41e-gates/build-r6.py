#!/usr/bin/env python3
"""Link a names-only signal observer into both kernel and static dyld slices."""
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
OUT = L / 'build-r6'
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
    # Reuse committed R5 abort hooks and libc archives; only sigexc changes.
    old_plan = json.loads((L/'build-r5/build-plan.json').read_text())
    replacements = {}
    for rule in ('C_COMPILER__libc-stdlib_unscanned_', 'C_COMPILER__libc-stdlib_dyld_unscanned_',
                 'C_COMPILER__emulation_unscanned_', 'C_COMPILER__emulation_dyld_unscanned_'):
        for row in recipe(rule):
            candidate = L/'build-r5'/(rule.split('__')[1]+'-'+Path(row['file']).name+'.o')
            if candidate.is_file(): replacements[row['output']] = str(candidate)
    for rule in ('C_STATIC_LIBRARY_LINKER__libc_static_', 'ASM_STATIC_LIBRARY_LINKER__libc_static32_',
                 'ASM_STATIC_LIBRARY_LINKER__libc_static64_'):
        row, = recipe(rule)
        replacements[row['output']] = str(L/'build-r5'/(Path(row['output']).stem+'-v2.a'))
    nodes = []
    inputs = dict(old_plan['inputs'])
    inputs.update({str(p): sha(p) for p in headers})
    inputs.update({v:sha(v) for v in replacements.values()})
    origin = Path(__file__).with_name('signal-origin.h')
    inputs[str(origin)] = sha(origin)
    manifest = L/'stage/qvolution-runtime-source.json'
    overrides = L/'runtime-overrides-r5.json'
    names = set(json.loads(manifest.read_text())['fingerprints'])
    names.update(json.loads(overrides.read_text()))
    # Exact paths only: never identify foreign code by a matching basename.
    images = sorted({(str((L/'stage'/name).resolve(strict=True)), Path(name).name)
                     for name in names if '/libexec/darling/' in name})
    config = OUT/'signal-images.h'
    config.write_text('static const char *const qvsImages[][2] = {\n' +
        ''.join('    {'+json.dumps(path)+', '+json.dumps(name)+'},\n' for path,name in images) +
        '};\n#define QVS_RDONLY '+str(os.O_RDONLY|os.O_CLOEXEC)+'\n#define QVS_APPEND '+
        str(os.O_WRONLY|os.O_APPEND|os.O_CLOEXEC|os.O_NOFOLLOW|os.O_NONBLOCK)+'\n')
    inputs[str(config)] = sha(config)
    inputs[str(manifest)] = sha(manifest)
    inputs[str(overrides)] = sha(overrides)
    def node(output, commands, dependencies):
        path = OUT/(Path(output).name + '.commands.json')
        path.write_text(json.dumps(commands, indent=2)+'\n')
        nodes.append((str(output), path, [str(Path(__file__)), *map(str, dependencies)]))
    source_base = L.parent/'stage41e/superproject/src/external/xnu/darling/src/libsystem_kernel/emulation/src/linux_premigration/signal/sigexc.c'
    inputs[str(source_base)] = sha(source_base)
    source = OUT/'sigexc-r6.c'
    original = source_base.read_text()
    entry = 'void sigexc_handler(int linux_signum, struct linux_siginfo* info, struct linux_ucontext* ctxt)\n{'
    assert original.count(entry) == 1
    hook = r"""
#define QVS_OPEN(path, flags) LINUX_SYSCALL(__NR_openat, -100, path, flags, 0)
#define QVS_READ(fd, buffer, size) LINUX_SYSCALL(__NR_read, fd, buffer, size)
#define QVS_WRITE(fd, buffer, size) LINUX_SYSCALL(__NR_write, fd, buffer, size)
#define QVS_CLOSE(fd) LINUX_SYSCALL(__NR_close, fd)
#include "signal-origin.h"
"""
    call = r"""
    if (linux_signum == LINUX_SIGSEGV && ctxt) {
#if defined(__x86_64__)
        qvsSignalOrigin((uintptr_t)ctxt->uc_mcontext.gregs.rip);
#elif defined(__i386__)
        qvsSignalOrigin((uintptr_t)ctxt->uc_mcontext.gregs.eip);
#elif defined(__aarch64__) || defined(__arm64__)
        qvsSignalOrigin((uintptr_t)ctxt->uc_mcontext.gregs.pc);
#endif
    }
"""
    source.write_text(original.replace(entry, hook+entry+call))
    inputs[str(source)] = sha(source)
    for rule in ('C_COMPILER__emulation_unscanned_', 'C_COMPILER__emulation_dyld_unscanned_'):
        row, = [r for r in recipe(rule) if r['file'].endswith('/sigexc.c')]
        output = OUT/(rule.split('__')[1]+'-sigexc.c.o')
        command = shlex.split(row['command'])
        command[1:1] = ['-I', str(OUT), '-I', str(origin.parent)]
        command[-1] = str(source)
        for flag in ('-o', '-MF', '-MT'):
            command[command.index(flag)+1] = str(output) + ('.d' if flag == '-MF' else '')
        replacements[row['output']] = str(output)
        node(output, [command], [source, origin, config])

    payloads = {}
    for rule in ('C_SHARED_LIBRARY_LINKER__system_kernel_',
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
        str(L/'superproject/tests/stub-forwarding/guest.m'): str(Path(__file__).with_name('segv-probe.c')),
        str(L/'build-r3/stub-forwarding.o'): str(OUT/'segv-probe.o'),
        str(L/'build-r3/stub-forwarding'): str(OUT/'segv-probe'),
    }
    commands = [[substitutions.get(arg, arg) for arg in step] for step in probe_commands]
    commands[0][1:1] = ['-x', 'objective-c']
    node(OUT/'segv-probe', commands, [Path(__file__).with_name('segv-probe.c')])
    inputs[str(Path(__file__).with_name('segv-probe.c'))] = sha(Path(__file__).with_name('segv-probe.c'))
    payloads['segv-probe'] = str(OUT/'segv-probe')
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
