#!/usr/bin/env python3
"""gpu-present: rebuild the changed Stage41c3 images from the lane clones and install them
into a hardlink-farm overlay of W/stage41c3.

Phases (each under qv-slot build; never starts Darling):
  build     replay Stage41c's own compile/link command lines (read from its ninja graph)
            into L/out. An object whose source and every recorded dependency are
            byte-identical to Stage41c's tree is copied from parallel/stage41c/build;
            every other object is recompiled from the lane clone. Each library is also
            relinked from Stage41c's own objects and must reproduce the installed
            Stage41c3 image byte for byte, which proves the replayed link is faithful.
  stage     L/stage = hardlink farm of W/stage41c3 (cp -al); every changed image is
            renamed aside and written as a new file, never written through a link.
  fixtures  compile and link the lane's guest fixtures against L/stage.
"""
from pathlib import Path
import hashlib, json, os, shlex, shutil, subprocess, sys

W = Path('/home/sean/Documents/Codex/Projects/Qvolution-work')
L = W / 'parallel/gpu-present'
S41 = W / 'parallel/stage41c/superproject'
B = W / 'parallel/stage41c/build'
BASE_STAGE = W / 'stage41c3'
O = L / 'out'
STAGE = L / 'stage'
TREES = {  # stage41c path prefix -> lane clone; only these subtrees are rebuilt from the lane
    str(S41 / 'src/external/cocotron'): L / 'cocotron',
    str(S41 / 'src/frameworks/OpenGL'): L / 'superproject/src/frameworks/OpenGL',
}
SYSROOT = 'usr/local/libexec/darling/'
TARGETS = [  # ninja target, installed path below SYSROOT
    ('src/external/cocotron/Onyx2D/Onyx2D', 'System/Library/PrivateFrameworks/Onyx2D.framework/Onyx2D'),
    ('src/frameworks/OpenGL/OpenGL', 'System/Library/Frameworks/OpenGL.framework/OpenGL'),
    ('src/external/cocotron/CoreGraphics/CoreGraphics', 'System/Library/Frameworks/CoreGraphics.framework/CoreGraphics'),
    ('src/external/cocotron/CoreGraphics/Wayland.backend/Wayland',
     'System/Library/Frameworks/CoreGraphics.framework/Resources/Backends/Wayland.backend/Contents/MacOS/Wayland'),
    ('src/external/cocotron/CoreGraphics/X11.backend/X11',
     'System/Library/Frameworks/CoreGraphics.framework/Resources/Backends/X11.backend/Contents/MacOS/X11'),
    ('src/external/cocotron/QuartzCore/QuartzCore', 'System/Library/Frameworks/QuartzCore.framework/QuartzCore'),
    ('src/external/cocotron/AppKit/AppKit', 'System/Library/Frameworks/AppKit.framework/AppKit'),
    ('src/external/cocotron/AppKit/Wayland',
     'System/Library/Frameworks/AppKit.framework/Resources/Backends/Wayland.backend/Contents/MacOS/Wayland'),
    ('src/external/cocotron/AppKit/X11',
     'System/Library/Frameworks/AppKit.framework/Resources/Backends/X11.backend/Contents/MacOS/X11'),
]
FIXTURES = [  # (source in the cocotron clone, binary name)
    ('tests/appkit-egl-presentation.m', 'appkit-egl-presentation'),
    ('tests/appkit-gpu-present-bench.m', 'appkit-gpu-present-bench'),
    ('tests/quartzcore-layer-gpu-composite.m', 'quartzcore-layer-gpu-composite'),
    ('tests/appkit-layer-backed-composite.m', 'appkit-layer-backed-composite'),
    ('tests/appkit-theme-gpu-frame.m', 'appkit-theme-gpu-frame'),
    ('tests/appkit-damage-redraw.m', 'appkit-damage-redraw'),
]
_sha = {}


def sha(p):
    p = str(p)
    if p not in _sha:
        with open(p, 'rb') as f:
            _sha[p] = hashlib.file_digest(f, 'sha256').hexdigest()
    return _sha[p]


def git(repo, *a):
    return subprocess.check_output(['git', '-C', str(repo), *a], text=True).strip()


def out(args, **kw):
    return subprocess.check_output(list(map(str, args)), text=True, **kw)


def lane_path(path):
    """The lane-clone counterpart of a Stage41c source path, or None if not rebuilt here.
    Headers are reached through framework-include symlinks, so compare real paths."""
    path = os.path.realpath(path)
    for prefix, mine in TREES.items():
        if path == prefix or path.startswith(prefix + '/'):
            return str(mine) + path[len(prefix):]
    return None


def include_overlay():
    """framework-include entries that resolve into a rebuilt subtree, pointed at the lane clone.
    Compile lines search this before Stage41c's framework-include."""
    overlay = O / 'include-overlay'
    overlay.mkdir(parents=True, exist_ok=True)
    for top in ('framework-include', 'framework-private-include'):
        for entry in sorted((S41 / top).iterdir()):
            mine = lane_path(str(entry))
            if mine and not (overlay / entry.name).exists():
                (overlay / entry.name).symlink_to(mine)
    return '-I' + str(overlay)


def rewrite(arg):
    for prefix, mine in TREES.items():
        arg = arg.replace(prefix + '/', str(mine) + '/')
    return arg


def record_deps(objects):
    """Dependencies ninja recorded for each object; every object must have a valid record."""
    result, current = {}, None
    for line in out(['ninja', '-C', B, '-t', 'deps', *objects]).splitlines():
        if line and not line.startswith(' '):
            current = line.split(':', 1)[0]
            assert '(VALID)' in line, line
            result[current] = []
        elif line.strip():
            d = line.strip()
            result[current].append(d if os.path.isabs(d) else str(B / d))
    assert set(result) == set(objects), 'missing dependency records'
    return result


def heads():
    return {'cocotron': git(L / 'cocotron', 'rev-parse', 'HEAD'), 'superproject': git(L / 'superproject', 'rev-parse', 'HEAD')}


assert os.environ.get('QV_SLOT', '').startswith('build-'), 'qv-slot build required'
for repo in ('cocotron', 'superproject'):
    assert not git(L / repo, 'status', '--porcelain', '--untracked-files=no'), repo + ': commit before building'
phase = sys.argv[1] if len(sys.argv) > 1 else ''
O.mkdir(exist_ok=True)

if phase == 'build':
    graph = ['rule run', '  command = cd ' + shlex.quote(str(B)) + ' && $cmd', '  description = $desc',
             'rule reuse', '  command = cp --reflink=auto $in $out', '  description = reuse $out']
    report = dict(heads=heads(), libraries={}, reused=0, recompiled=[])
    overlay = include_overlay()
    outputs = {t: str(O / ('candidate-' + t.replace('/', '_'))) for t, _ in TARGETS}
    for target, installed in TARGETS:
        commands = [shlex.split(c) for c in out(['ninja', '-C', B, '-t', 'commands', target]).splitlines()]
        link = next(a for a in commands if '-o' in a and a[a.index('-o') + 1] == target)
        assert link[:2] == [':', '&&'] and link[-2:] == ['&&', ':'], target
        link = link[2:-2]
        assert '&&' not in link, target
        compiles = {a[a.index('-o') + 1]: a for a in commands if '-c' in a and '-o' in a}
        recorded = record_deps([a for a in link if a.endswith('.o')])
        candidate, baseline_objects = list(link), []
        for i, arg in enumerate(link):
            if not arg.endswith('.o'):
                continue
            src = compiles[arg][compiles[arg].index('-c') + 1]
            deps = recorded[arg]
            changed = [d for d in [src, *deps] if lane_path(d) and (not os.path.exists(lane_path(d)) or sha(lane_path(d)) != sha(d))]
            obj = O / ('obj-' + arg.replace('/', '_'))
            if not changed:
                graph += ['build %s: reuse %s' % (obj, B / arg)]
                report['reused'] += 1
            else:
                args = [rewrite(a) for a in compiles[arg]]
                # Embedded source paths (assert, __FILE__) stay Stage41c's, so an object
                # recompiled only for a changed header can come out byte-identical.
                args[1:1] = [overlay] + ['-ffile-prefix-map=%s=%s' % (mine, prefix) for prefix, mine in TREES.items()]
                for flag, value in [('-o', obj), ('-MF', str(obj) + '.d'), ('-MT', obj)]:
                    args[args.index(flag) + 1] = str(value)
                graph += ['build %s: run %s' % (obj, lane_path(src) or src),
                          '  cmd = ' + shlex.join(args).replace('$', '$$'), '  desc = ' + obj.name,
                          '  depfile = %s.d' % obj, '  deps = gcc']
                report['recompiled'].append(arg)
            candidate[i] = str(obj)
            baseline_objects.append(str(B / arg))
        # perf/damage adds a plain-C region implementation absent from the
        # baseline graph. Compile it with Onyx2D's own flags; baseline linking
        # deliberately keeps the original object list for reproduction proof.
        if target == 'src/external/cocotron/Onyx2D/Onyx2D':
            src = L / 'cocotron/Onyx2D/O2SurfaceDamage.m'
            obj = O / 'obj-O2SurfaceDamage.m.o'
            template = next(a for a in compiles.values() if a[a.index('-c') + 1].endswith('/O2Surface.m'))
            args = [rewrite(a) for a in template]
            args[args.index('-c') + 1] = str(src)
            args[1:1] = [overlay]
            for flag, value in [('-o', obj), ('-MF', str(obj) + '.d'), ('-MT', obj)]:
                args[args.index(flag) + 1] = str(value)
            graph += ['build %s: run %s' % (obj, src),
                      '  cmd = ' + shlex.join(args).replace('$', '$$'), '  desc = ' + obj.name,
                      '  depfile = %s.d' % obj, '  deps = gcc']
            candidate.insert(candidate.index('-o'), str(obj))
            report['recompiled'].append('Onyx2D/O2SurfaceDamage.m (new)')
        deps = [a for a in candidate if a.endswith('.o')]
        for other, path in outputs.items():
            if other != target and other in candidate:
                candidate = [path if a == other else a.replace(str(B / other), path) for a in candidate]
                deps.append(path)
        candidate[candidate.index('-o') + 1] = outputs[target]
        baseline = list(link)
        baseline[baseline.index('-o') + 1] = str(O / ('baseline-' + target.replace('/', '_')))
        for name, args, inputs in [(outputs[target], candidate, deps),
                                   (baseline[baseline.index('-o') + 1], baseline, baseline_objects)]:
            # A link line with absolute object paths exceeds the kernel's single-argument
            # limit for AppKit; hand the arguments to the driver in a response file.
            rsp = Path(name + '.rsp')
            rsp.write_text(shlex.join(args[1:]) + '\n')
            graph += ['build %s: run %s %s' % (name, ' '.join(inputs), rsp),
                      '  cmd = ' + shlex.join([args[0], '@' + str(rsp)]).replace('$', '$$'), '  desc = ' + Path(name).name]
        report['libraries'][target] = dict(installed=SYSROOT + installed, candidate=outputs[target],
                                           baseline=baseline[baseline.index('-o') + 1])
    (O / 'build.ninja').write_text('\n'.join(graph) + '\n')
    subprocess.run(['ninja', '-C', str(O), '-j6'], check=True)
    dry = out(['ninja', '-C', O, '-n'])
    (O / 'ninja-dry-run.log').write_text(dry)
    assert 'no work to do' in dry, dry
    for target, row in report['libraries'].items():
        row['baseline_sha256'] = sha(row['baseline'])
        row['installed_sha256'] = sha(BASE_STAGE / row['installed'])
        assert row['baseline_sha256'] == row['installed_sha256'], target + ': replayed link does not reproduce Stage41c3'
        row['candidate_sha256'] = sha(row['candidate'])
    assert report['heads'] == heads(), 'source moved during build'
    for repo in ('cocotron', 'superproject'):
        assert not git(L / repo, 'status', '--porcelain', '--untracked-files=no'), repo + ': edited during build'
    (O / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS reused=%d recompiled=%d changed images=%s' % (
        report['reused'], len(report['recompiled']),
        [t for t, r in report['libraries'].items() if r['candidate_sha256'] != r['installed_sha256']]))

elif phase == 'stage':
    report = json.loads((O / 'report.json').read_text())
    assert report['heads'] == heads(), 'stage only what was built from the current heads'
    assert 'no work to do' in out(['ninja', '-C', O, '-n'])
    # One overlay per built head; L/stage points at the newest. Nothing is deleted.
    target = L / ('stage-' + report['heads']['cocotron'][:10] + '-' + report['heads']['superproject'][:10])
    assert not target.exists(), 'already staged: ' + str(target)
    assert not STAGE.exists() or STAGE.is_symlink(), 'L/stage must be a symlink'
    subprocess.run(['cp', '-al', str(BASE_STAGE), str(target)], check=True)
    link = L / 'stage.new'
    link.symlink_to(target.name)
    link.replace(STAGE)
    replaced = {}
    for target, row in report['libraries'].items():
        if row['candidate_sha256'] == row['installed_sha256']:
            continue
        dest = (STAGE / row['installed']).resolve()
        assert dest.is_relative_to(STAGE.resolve()), dest
        dest.rename(dest.with_name(dest.name + '.stage41c3'))
        shutil.copyfile(row['candidate'], dest)
        dest.chmod(0o755)
        assert sha(dest) == row['candidate_sha256']
        replaced[row['installed']] = row['candidate_sha256']
    manifest_path = STAGE / 'qvolution-runtime-source.json'
    manifest = json.loads(manifest_path.read_text())
    manifest_path.rename(manifest_path.with_name('qvolution-runtime-source.stage41c3.json'))
    for rel in manifest['fingerprints']:
        manifest['fingerprints'][rel] = sha(STAGE / rel)
    manifest.update(stage_label='gpu-present', base_stage=str(BASE_STAGE), overlay='hardlink farm (cp -al)',
                    replaced_images=replaced, built_heads=report['heads'],
                    build_receipt=dict(path=str(O / 'report.json'), sha256=sha(O / 'report.json')))
    manifest['components']['cocotron'] = report['heads']['cocotron']
    manifest['component_paths']['src/external/cocotron'] = report['heads']['cocotron']
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    # Every replaced image is a fresh inode; stage41c3 itself is unchanged.
    for rel, digest in replaced.items():
        p = (STAGE / rel).resolve()
        q = (BASE_STAGE / rel).resolve()
        assert p.stat().st_ino != q.stat().st_ino and sha(q) == report['libraries'][next(
            t for t, r in report['libraries'].items() if r['installed'] == rel)]['installed_sha256']
    (L / 'evidence').mkdir(exist_ok=True)
    (L / 'evidence/install.json').write_text(json.dumps(dict(stage=str(STAGE), heads=report['heads'], replaced=replaced), indent=2) + '\n')
    print('PASS staged', STAGE, json.dumps(replaced, indent=1))

elif phase == 'fixtures':
    report = json.loads((O / 'report.json').read_text())
    assert report['heads'] == heads()
    root = STAGE / SYSROOT
    C = L / 'cocotron'
    commands = [shlex.split(c) for c in out(['ninja', '-C', B, '-t', 'commands', 'src/external/cocotron/AppKit/AppKit']).splitlines()]
    template = next(a for a in commands if '-c' in a and a[a.index('-c') + 1].endswith('/AppKit/NSColor.subproj/NSColor.m'))
    graph = ['rule run', '  command = cd ' + shlex.quote(str(B)) + ' && $cmd', '  description = $desc']
    built = {}
    for rel, name in FIXTURES:
        args = [rewrite(a) for a in template]
        obj, binary = O / (name + '.o'), O / name
        args[args.index('-c') + 1] = str(C / rel)
        for flag, val in [('-o', obj), ('-MF', str(obj) + '.d'), ('-MT', obj)]:
            args[args.index(flag) + 1] = str(val)
        args[1:1] = [include_overlay(), '-UNDEBUG', '-I' + str(C / 'AppKit')]
        ld = [B / 'src/external/cctools-port/cctools/ld64/src/ld', '-syslibroot', root, '-arch', 'x86_64',
              '-macosx_version_min', '11.0', '-L' + str(root / 'usr/lib'), '-L' + str(root / 'usr/lib/swift'),
              '-F' + str(root / 'System/Library/Frameworks'), '-F' + str(root / 'System/Library/PrivateFrameworks'),
              '-rpath', '/usr/lib/swift', '-lSystem', '-lobjc']
        for f in ['CoreFoundation', 'Foundation', 'CoreGraphics', 'Onyx2D', 'OpenGL', 'QuartzCore', 'AppKit']:
            ld += ['-framework', f]
        ld += [obj, '-o', binary]
        for t, a, i in [(obj, args, [C / rel]), (binary, ld, [obj])]:
            graph += ['build %s: run %s' % (t, ' '.join(map(str, i))),
                      '  cmd = ' + shlex.join(list(map(str, a))).replace('$', '$$'), '  desc = ' + Path(t).name]
        built[name] = dict(source=rel, binary=str(binary), source_sha256=sha(C / rel))
    (O / 'fixtures.ninja').write_text('\n'.join(graph) + '\n')
    subprocess.run(['ninja', '-C', str(O), '-f', 'fixtures.ninja', '-j6'], check=True)
    assert 'no work to do' in out(['ninja', '-C', O, '-f', 'fixtures.ninja', '-n'])
    for r in built.values():
        r['sha256'] = sha(r['binary'])
    (O / 'fixtures.json').write_text(json.dumps(dict(heads=report['heads'], fixtures=built), indent=2) + '\n')
    print('PASS fixtures built', ' '.join(built))
else:
    raise SystemExit('build|stage|fixtures')
