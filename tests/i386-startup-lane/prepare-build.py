#!/usr/bin/env python3
"""Replay Stage41d's build commands into this lane, never mutate its build."""
from pathlib import Path
import hashlib, json, shlex, subprocess
L = Path('/home/sean/Documents/Codex/Projects/Qvolution-work/parallel/i386-guest')
B = L / 'build'
B.mkdir(exist_ok=True)
S = L / 'superproject'
BASE = L.parent / 'stage41d'
shared = BASE / 'build'
old = BASE / 'superproject'
header = S / 'src/external/xnu/darling/src/libsystem_kernel/emulation/include/common/i386-startup.h'
overlay = B / 'include'
(overlay / 'darling').mkdir(parents=True, exist_ok=True)
if not (overlay / 'darling/emulation').exists():
    (overlay / 'darling/emulation').symlink_to(header.parent.parent, target_is_directory=True)
steps = {}
edges = []
mapped = {}

def compdb(*rules):
    return json.loads(subprocess.check_output(['ninja', '-C', str(shared), '-t', 'compdb', '-x', *rules]))

def edge(name, output, command, deps=()):
    name += '-' + hashlib.sha256(json.dumps(command).encode()).hexdigest()[:12]
    steps[name] = [command]
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    edges.append(f'build {output}: step ' + ' '.join(map(str, deps)) + f'\n  key = {name}\n')

def clean(args):
    for flag in ('-MD', '-MT', '-MF'):
        while flag in args:
            i = args.index(flag)
            del args[i:i + (1 if flag == '-MD' else 2)]
    return args

rules = ['C_COMPILER__mldr_unscanned_', 'C_COMPILER__mldr32_unscanned_',
         'C_COMPILER__darling_unscanned_', 'C_COMPILER__mldr_dserver_rpc_unscanned_', 'C_COMPILER__mldr32_dserver_rpc_unscanned_',
         'C_COMPILER__libsyscall_unscanned_', 'C_COMPILER__libsyscall_dynamic_unscanned_',
         'ASM_COMPILER__libsyscall_unscanned_',
         'CXX_COMPILER__system_loader_unscanned_',
         'C_COMPILER__emulation_unscanned_', 'C_COMPILER__emulation_dyld_unscanned_']
for row in compdb(*rules):
    original = Path(row['file'])
    is_mldr = row['output'].startswith('src/startup/mldr/')
    is_launcher = row['output'] == 'src/startup/CMakeFiles/darling.dir/darling.c.o'
    wanted = original.name in ('mach_init.c', 'mig_reply_port.c', '_libkernel_init.c', 'dyldInitialization.cpp', 'dyld2.cpp', 'mach_traps.c', 'tls.c') or (original.name == 'rpc.c' and not is_mldr) or (original.suffix == '.S' and '/libsyscall/' in str(original))
    if not is_mldr and not is_launcher and not wanted:
        continue
    a = clean(shlex.split(row['command']))
    source = original
    if wanted:
        resolved = original.resolve()
        source = S / resolved.relative_to(old) if resolved.is_relative_to(old) else original
    elif str(original).startswith(str(old)):
        source = S / original.relative_to(old)
    output = B / row['output']
    mapped[row['output']] = str(output)
    a[a.index('-o') + 1] = str(output)
    a[a.index('-c') + 1] = str(source)
    # Prefer our emulation headers; retain Stage41d SDK/generated include paths.
    a[1:1] = ['-I' + str(overlay)]
    a = [arg.replace(str(old / 'src/external/xnu/darling/src/libsystem_kernel/emulation/include/linux_premigration/resources/dserver-rpc-defs.h'), str(S / 'src/external/xnu/darling/src/libsystem_kernel/emulation/include/linux_premigration/resources/dserver-rpc-defs.h')) for arg in a]
    if is_mldr and original.name == 'rpc.c':
        a[1:1] = ['-pthread']
    edge('compile-' + str(len(steps)), output, a, [source, header, S / 'src/external/xnu/libsyscall/custom/SYS.h', S / 'src/external/xnu/gen/bsdsyscalls/SYS.h', S / 'src/external/xnu/darling/src/libsystem_kernel/emulation/include/linux_premigration/resources/dserver-rpc-defs.h', *([S / 'src/startup/mldr/stack.c', S / 'src/startup/mldr/loader.c', S / 'src/startup/mldr/tsd.h'] if is_mldr else [])])

launcher_object = mapped['src/startup/CMakeFiles/darling.dir/darling.c.o']
edge('darling', B / 'darling', ['/usr/bin/clang', launcher_object, '-lutil', '-o', str(B / 'darling')], [launcher_object])

for bits in (64, 32):
    target = 'mldr' if bits == 64 else 'mldr32'
    objects = [v for k, v in mapped.items() if f'/{target}.dir/' in k or f'/{target}_dserver_rpc.dir/' in k]
    assert len(objects) == 5, (target, objects)
    edge(target, B / target, ['/usr/bin/clang', *(['-m32'] if bits == 32 else []), '-pthread', *objects, '-lrt', '-ldl', '-lm', '-o', str(B / target)], objects)

for row in compdb('C_STATIC_LIBRARY_LINKER__system_kernel_static32_', 'C_STATIC_LIBRARY_LINKER__system_kernel_static64_'):
    a = shlex.split(row['command'])
    start = next(i for i, arg in enumerate(a) if arg.endswith('-ar'))
    end = a.index('&&', start)
    a = a[start:end]
    output = B / row['output']
    mapped[row['output']] = str(output)
    a[2] = str(output)
    deps = [mapped[arg] for arg in a if arg in mapped]
    a = [mapped.get(arg, arg) for arg in a]
    edge('archive-' + output.stem, output, ['archive', *a], deps)

for row in compdb('CXX_EXECUTABLE_LINKER__system_loader_', 'C_SHARED_LIBRARY_LINKER__system_kernel_'):
    a = shlex.split(row['command'])
    assert a[:2] == [':', '&&'] and a[-2:] == ['&&', ':']
    a = a[2:-2]
    output = B / row['output']
    a[a.index('-o') + 1] = str(output)
    deps = [mapped[arg] for arg in a if arg in mapped]
    a = [mapped.get(arg, arg) for arg in a]
    rsp = B / (output.name + '.link.rsp')
    rsp.write_text(shlex.join(a[1:]) + '\n')
    edge('link-' + output.name, output, [a[0], '@' + str(rsp)], deps)

for bits, arch in ((32, 'i386'), (64, 'x86_64')):
    edge('host-' + arch, B / ('host-' + arch), ['/usr/bin/clang', f'-m{bits}', '-nostdlib', '-static', '-Wl,--build-id=none', str(L / 'host-ia32.S'), '-o', str(B / ('host-' + arch))], [L / 'host-ia32.S'])

edge('stack-layout', B / 'stack-layout', ['/usr/bin/clang', '-Wno-pointer-to-int-cast', '-Wno-format', '-fsanitize=address,undefined', '-I' + str(shared / 'src/include'), str(S / 'tests/mldr-i386-stack.c'), '-o', str(B / 'stack-layout')], [S / 'tests/mldr-i386-stack.c', S / 'src/startup/mldr/stack.c'])
edge('record-gate', B / 'record-gate', ['/usr/bin/clang', '-Wall', '-Wextra', '-Werror', '-I' + str(overlay), str(S / 'src/external/xnu/darling/tests/i386-startup-records.c'), '-o', str(B / 'record-gate')], [header, S / 'src/external/xnu/darling/tests/i386-startup-records.c'])
maps = [arg for arg in shlex.split(compdb('C_SHARED_LIBRARY_LINKER__system_kernel_')[0]['command']) if arg.startswith('-Wl,-dylib_file,')]
# A hello linked only to this runtime's libSystem exercises dyld and constructors.
fixture = L / 'hello.c'
fixture.write_text('#include <unistd.h>\nint main(void) { static const char message[] = "i386-guest hello\\n"; return write(1, message, sizeof(message)-1) == sizeof(message)-1 ? 0 : 1; }\n')
for arch in ('i386', 'x86_64'):
    cmd = ['/usr/bin/clang', '-target', arch + '-apple-darwin20', '-arch', arch, '-mmacosx-version-min=10.10', '-nostdinc', '-isystem', '/usr/lib/llvm-21/lib/clang/21/include', '-I' + str(old / 'basic-headers'), '-I' + str(old / 'Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include'), '-c', str(fixture), '-o', str(B / ('hello-' + arch + '.o'))]
    layout = S / 'tests/i386-startup-r3/rpc-layout.c'
    layout_cmd = cmd[:]
    layout_cmd[1:1] = ['-I' + str(overlay), '-I' + str(shared / 'src/external/darlingserver/include')]
    layout_cmd[layout_cmd.index('-c') + 1] = str(layout)
    layout_cmd[layout_cmd.index('-o') + 1] = str(B / ('rpc-layout-' + arch + '.o'))
    edge('rpc-layout-' + arch, B / ('rpc-layout-' + arch + '.o'), layout_cmd, [layout])
    edge('hello-compile-' + arch, B / ('hello-' + arch + '.o'), cmd, [fixture])
    cmd = ['/usr/bin/clang', '-target', arch + '-apple-darwin20', '-arch', arch, '-mmacosx-version-min=10.10', '-nostdlib', '-fuse-ld=' + str(shared / 'src/external/cctools-port/cctools/ld64/src/x86_64-apple-darwin20-ld'), '-Wl,-Z', *maps, str(B / ('hello-' + arch + '.o')), str(shared / 'src/external/libsystem/libSystem.B.dylib'), '-o', str(B / ('hello-' + arch))]
    edge('hello-link-' + arch, B / ('hello-' + arch), cmd, [B / ('hello-' + arch + '.o')])

(B / 'steps.json').write_text(json.dumps(steps, indent=2) + '\n')
(B / 'build.ninja').write_text('rule step\n  command = python3 ' + str(L / 'run-build-step.py') + ' $key\n  description = $key\n' + '\n'.join(edges))
print('Prepared', len(steps), 'isolated build steps')
