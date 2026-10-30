#!/usr/bin/env python3
"""Qvolution's local compatibility queue and reproducible host checks."""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT.parent / (ROOT.name + '-work')

def command(args, cwd=ROOT, **kwargs):
    return subprocess.run(args, cwd=cwd, text=True, capture_output=True, **kwargs)

def apps():
    data = json.loads((ROOT / 'qvolution/apps.json').read_text())
    rows = data['apps']
    if len(rows) != 20 or len({a['id'] for a in rows}) != 20:
        raise ValueError('expected 20 distinct app targets')
    return rows

def snapshot():
    revision = command(['git', 'rev-parse', 'HEAD'], check=True).stdout.strip()
    state = command(['git', 'status', '--porcelain'], check=True).stdout.splitlines()
    modules = command(['git', 'submodule', 'status', '--recursive'], check=True).stdout.splitlines()
    return {'superproject': revision, 'dirty': bool(state), 'changes': state,
            'submodules': [{'state': s[0], 'sha': s[1:].split()[0],
                            'path': s[1:].split()[1]} for s in modules]}

def doctor():
    tools = ['git', 'git-lfs', 'cmake', 'ninja', 'clang', 'clang++', 'bison',
             'flex', 'ruby', 'pkg-config', 'wayland-scanner']
    missing = [x for x in tools if not shutil.which(x)]
    modules = snapshot()['submodules']
    mismatches = [m for m in modules if m['state'] != ' ']
    pointers = []
    swift = ROOT / 'src/external/swift'
    if swift.is_dir():
        for p in swift.rglob('*'):
            if p.is_file() and p.stat().st_size < 1024:
                if p.read_bytes().startswith(b'version https://git-lfs.github.com/spec/v1'):
                    pointers.append(str(p.relative_to(ROOT)))
    result = {'host_arch': platform.machine(), 'missing_tools': missing,
              'submodules': len(modules), 'submodule_mismatches': mismatches,
              'swift_lfs_pointers_remaining': pointers,
              'runtime_build_complete': None,
              'note': 'Tool/source checks only. Build and staged-runtime tests are separate gates.'}
    print(json.dumps(result, indent=2))
    return 1 if missing or mismatches or pointers else 0

def host_tests():
    WORK.mkdir(exist_ok=True)
    results = []
    env = os.environ.copy()
    # LeakSanitizer needs ptrace permissions unavailable in some Codex environments.
    # Explicitly record this limitation; ASan and UBSan remain active where compiled.
    env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
    tests = [
        ('canonical-submodule-checkout', [sys.executable, 'scripts/ci/test_checkout_submodules.py']),
        ('sandbox-stub-contract', [sys.executable, 'tests/sandbox-honest-failure.py']),
        ('vimage-scale', [sys.executable, 'tests/vimage-scale-argb8888.py']),
        ('colorsync-constants', [sys.executable, 'tests/colorsync-device-constants.py']),
        ('cfnetwork-bootstrap', [sys.executable, 'tests/cfnetwork-bootstrap.py']),
        ('roon-links', [sys.executable, '-m', 'unittest', 'discover', '-s', 'tests/roon-assembly-links']),
    ]
    def run(name, args):
        try:
            proc = command(args, env=env, timeout=120)
            item = {'name': name, 'command': args, 'exit_code': proc.returncode,
                    'status': 'pass' if proc.returncode == 0 else 'fail',
                    'stdout': proc.stdout, 'stderr': proc.stderr}
        except (OSError, subprocess.TimeoutExpired) as exc:
            item = {'name': name, 'command': args, 'status': 'error', 'error': str(exc)}
        results.append(item)
        print(name + ': ' + item['status'], flush=True)
        return item['status'] == 'pass'
    for name, args in tests:
        run(name, args)
    with tempfile.TemporaryDirectory(prefix='host-tests-', dir=WORK) as tmp:
        for name, source in [
            ('prefix-state', 'tests/launcher/prefix-state.c'),
            ('profile-prefix', 'tests/launcher/profile-prefix.c'),
            ('fractional-geometry', 'src/external/cocotron/AppKit/Wayland.backend/tests/fractional-geometry.c'),
        ]:
            binary = str(Path(tmp) / name)
            if run(name + '-compile', ['clang', '-O1', '-g', '-Wall', '-Wextra',
                                      '-fsanitize=address,undefined', source, '-lm', '-o', binary]):
                run(name, [binary])
    record = {'created_utc': dt.datetime.now(dt.timezone.utc).isoformat(),
              'source': snapshot(), 'leak_checking': False,
              'scope': 'Host fixtures only; no guest app or compositor success is implied.',
              'results': results}
    path = WORK / 'host-tests.json'
    path.write_text(json.dumps(record, indent=2) + '\n')
    print('Evidence: ' + str(path))
    return int(any(r['status'] != 'pass' for r in results))

def new_run(app_id):
    app = next((a for a in apps() if a['id'] == app_id), None)
    if app is None:
        raise ValueError('unknown app id: ' + app_id)
    stamp = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    path = WORK / 'runs' / app_id / stamp
    path.mkdir(parents=True, exist_ok=False)
    record = {'app': app_id, 'created_utc': stamp, 'source': snapshot(),
              'app_version': None, 'app_architecture': None, 'artifact_sha256': None,
              'artifact_source': app['official_url'], 'artifact_unmodified': None,
              'host_architecture': platform.machine(), 'compositor_version': None,
              'backend': None, 'prefix': None, 'commands': [],
              'milestones': dict(app['milestones']),
              'workflow': [{'step': s, 'status': 'not_tested', 'evidence': None} for s in app['workflow']],
              'blockers': [], 'notes': ''}
    (path / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    print(path / 'result.json')
    return 0

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    for name in ['apps', 'doctor', 'host-tests', 'manifest', 'validate']:
        sub.add_parser(name)
    sub.add_parser('new-run').add_argument('app_id')
    args = parser.parse_args()
    if args.action == 'apps':
        for a in apps():
            print(f"{a['priority']:2}. {a['name']:<20} wave={a['wave']} {a['status']}")
    elif args.action == 'doctor':
        return doctor()
    elif args.action == 'host-tests':
        return host_tests()
    elif args.action == 'manifest':
        print(json.dumps(snapshot(), indent=2))
    elif args.action == 'new-run':
        return new_run(args.app_id)
    elif args.action == 'validate':
        targets = apps()
        for a in targets:
            assert (ROOT / 'qvolution/apps' / a['id'] / 'README.md').is_file(), a['id']
            assert a['milestones'].keys() == {'acquire', 'install', 'load', 'launch', 'workflow', 'gnome', 'stability'}
            assert len(a['workflow']) >= 3
        print('PASS: 20 distinct targets, playbooks and milestone records')
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, subprocess.CalledProcessError) as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
