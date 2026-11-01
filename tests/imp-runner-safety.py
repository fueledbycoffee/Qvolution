#!/usr/bin/env python3
"""Check runner policy, then exercise exit traps using an entirely fake host."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

runner, evidence, kind = sys.argv[1:]
runner = Path(runner).resolve()
source = runner.read_text()
assert 'qv-host-guard' in source and 'snapshot' in source and 'verify' in source, 'missing mandatory home guard'
assert 'cp -a --reflink' not in source, 'runner must reuse an approved stage'
assert 'rm ' not in source and '.unlink(' not in source and 'rmtree(' not in source, 'runner deletes artifacts'
base = Path(tempfile.mkdtemp(prefix='qvr-', dir='/tmp'))
Path(evidence, base.name + '.txt').write_text(str(base) + '\n')
w = base / 'w'
dashboard = w / 'dashboard'
dashboard.mkdir(parents=True)
def executable(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('#!/usr/bin/env bash\nset -eu\n' + text)
    path.chmod(0o755)
executable(dashboard / 'qv-slot', 'export QV_SLOT="$1-mock"; shift; exec "$@"\n')
executable(dashboard / 'qv-host-guard', '''echo "$1" >> "$EVENTS"
if [[ $1 == snapshot ]]; then : > "$2"; fi
if [[ $1 == verify && ${CHANGED:-0} == 1 ]]; then exit 3; fi
''')
lane = w / 'parallel' / kind
launcher = (lane / 'build/src/startup/darling') if kind == 'imp-system' else (w / 'parallel/stage41/build/src/startup/darling')
executable(launcher, '''if [[ $1 == shutdown ]]; then echo shutdown >> "$EVENTS"; exit 0; fi
echo boot >> "$EVENTS"
mkdir -p "$DPREFIX"
[[ ${FAIL:-0} == 0 ]] || exit 7
case "$*" in
  *websocket-options*) echo 'BEGIN websocket-options'; exit 132;;
  *websocket-metadata*) echo 'BEGIN websocket-metadata'; exit 132;;
  *OverlayBehavior*) echo 'RESULT swift-overlay-behavior failures=0';;
  *load-exports*) echo 'RESULT imp-system-load checked=32 failures=0';;
  *behavior*) echo 'RESULT imp-system-behavior failures=0';;
  *port-binding*) echo 'RESULT imp-system-port-binding failures=0';;
esac
''')
if kind == 'imp-system':
    (lane / 'fixtures').mkdir(parents=True)
    for name in ['load-exports', 'behavior', 'libsystem-control', 'cfnetwork-control', 'port-binding']:
        (lane / 'fixtures' / ('imp-system-' + name)).touch()
    manifest = lane / 'superproject/tests/imp-system-guest-exports.tsv'
    manifest.parent.mkdir(parents=True)
    manifest.touch()
    for component, name, version in [('corefoundation', 'CoreFoundation', 'A'), ('foundation', 'Foundation', 'C')]:
        for target in [lane / 'build/src/external' / component / name,
                       lane / 'stage/usr/local/libexec/darling/System/Library/Frameworks' / (name + '.framework') / 'Versions' / version / name]:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('own mock image')
else:
    env = dict(os.environ, QV_WORKSPACE=str(w), EVENTS=str(base / 'missing-events'))
    result = subprocess.run(['bash', str(runner)], env=env, capture_output=True, text=True)
    assert result.returncode == 2 and not (base / 'missing-events').exists()
    mock_bin = w / 'mock-bin'
    executable(mock_bin / 'git', 'exit 0\n')
    os.environ['PATH'] = str(mock_bin) + os.pathsep + os.environ['PATH']
    build = lane / 'build'
    for module in ['UniformTypeIdentifiers', 'AVFoundation', 'CoreFoundation', 'os', 'Network']:
        image = Path('libswift' + module + '.dylib')
        for directory in [build / module, lane / 'stage/usr/local/libexec/darling/usr/lib/swift']:
            directory.mkdir(parents=True, exist_ok=True)
            (directory / image).write_text('own mock image')
        exports = lane / 'superproject/src/external/swift/overlays' / module / 'imports.exports'
        exports.parent.mkdir(parents=True, exist_ok=True)
        exports.write_text('own mock demand')
    executable(build / 'load-exports', 'exit 0\n')
    executable(build / 'Behavior/OverlayBehavior', 'exit 0\n')
    (build / 'behavior.path').write_text(str(build / 'Behavior/OverlayBehavior'))
    (lane / 'evidence').mkdir(parents=True)
    (lane / 'evidence/latest-build.path').write_text(str(build))
(lane / 'evidence').mkdir(parents=True, exist_ok=True)
for failure, changed, expected in [(0, 0, 0), (1, 0, 1 if kind == 'imp-system' else 7), (0, 1, 3), (1, 1, 3)]:
    events = base / f'events-{failure}-{changed}'
    env = dict(os.environ, QV_WORKSPACE=str(w), EVENTS=str(events), FAIL=str(failure), CHANGED=str(changed))
    result = subprocess.run(['bash', str(runner)], env=env, capture_output=True, text=True)
    (base / f'run-{failure}-{changed}.log').write_text(result.stdout + result.stderr)
    assert result.returncode == expected, (result.returncode, expected, result.stdout, result.stderr)
    sequence = events.read_text().splitlines() if events.exists() else []
    assert sequence[0] == 'snapshot' and sequence[-2:] == ['shutdown', 'verify'], sequence
print('PASS runner safety and mocked exits; no Darling container used:', base)
