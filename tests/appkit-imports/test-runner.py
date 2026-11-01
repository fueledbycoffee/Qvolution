#!/usr/bin/env python3
"""Exercise the real shell runner with authored controls, never a container.
All fixtures are retained; --runner also accepts the pre-review lane script.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--runner', type=Path, default=Path(__file__).with_name('run-container-load.sh'))
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='runner-', dir=args.evidence.resolve()))
print('Retained controls:', root, flush=True)
source = args.runner.read_text()
legacy = 'W=/home/sean/Documents/Codex/Projects/Qvolution-work' in source
cases = ['success', 'probe-fail', 'initialize-fail', 'snapshot-fail', 'verify-fail', 'shutdown-fail', 'retry']
for case in cases:
    work = root / case
    lane = work / 'parallel/imp-appkit'
    for directory in ['evidence', 'probes', 'build/src/startup']:
        (lane / directory).mkdir(parents=True)
    (work / 'dashboard').mkdir()
    (work / 'p').mkdir()
    events = work / 'events.jsonl'
    def control(path, code):
        path.write_text('#!/usr/bin/env python3\n' + code)
        path.chmod(0o755)
    control(work / 'dashboard/qv-host-guard', '''import json, os, sys
from pathlib import Path
with open(os.environ['EVENTS'], 'a') as out: out.write(json.dumps(['guard', *sys.argv[1:]])+'\\n')
if sys.argv[1] == 'snapshot': Path(sys.argv[2]).write_text('snapshot\\n')
sys.exit(3 if os.environ['CASE'] == sys.argv[1]+'-fail' else 0)
''')
    control(lane / 'build/src/startup/darling', '''import json, os, sys
from pathlib import Path
assert not any(k.startswith('DYLD_') for k in os.environ)
assert os.environ['DARLING_NONROOT'] == '1'
prefix = Path(os.environ['DPREFIX'])
assert len(str(prefix)) <= 83
with open(os.environ['EVENTS'], 'a') as out: out.write(json.dumps(['launcher', *sys.argv[1:]])+'\\n')
case = os.environ['CASE']
if sys.argv[1] == 'shutdown': sys.exit(1 if case == 'shutdown-fail' else 0)
if sys.argv[2] == '/usr/bin/true':
    if case == 'initialize-fail': sys.exit(1)
    if case == 'retry' and not prefix.exists():
        prefix.mkdir(parents=True)
        sys.exit(1)
    if not prefix.exists(): prefix.mkdir(parents=True)
else:
    assert (prefix/'Applications/ImportProbes/marker').read_text() == 'own probes'
    if case == 'probe-fail': sys.exit(1)
    print('PASS authored probe control')
''')
    (lane / 'probes/marker').write_text('own probes')
    runner = work / 'runner.sh'
    runner.write_text(source.replace('W=/home/sean/Documents/Codex/Projects/Qvolution-work', 'W='+str(work)) if legacy else source)
    # Preflight is tested independently against the real lane artifacts.
    (work / 'preflight.py').write_text('')
    env = dict(os.environ, CASE=case, EVENTS=str(events), QV_SLOT='guest-test', DYLD_TEST='must be cleared')
    # Short aliases keep the fixture prefix beneath the production socket limit.
    short = Path(tempfile.mkdtemp(prefix='ia-', dir='/tmp'))
    (short/'p').symlink_to(work/'p', target_is_directory=True)
    runner.write_text(runner.read_text().replace('export DPREFIX="$W/p/ia-$RANDOM-$$"', 'export DPREFIX="'+str(short)+'/p/ia-$RANDOM-$$"'))
    command = ['bash', str(runner), '--guest'] if legacy else ['bash', str(runner), str(lane), '--guest']
    result = subprocess.run(command, env=env, capture_output=True, text=True)
    (work/'stdout.log').write_text(result.stdout)
    (work/'stderr.log').write_text(result.stderr)
    rows = [json.loads(row) for row in events.read_text().splitlines()] if events.exists() else []
    assert rows and rows[0][:2] == ['guard', 'snapshot'], (case, 'missing snapshot before boot', rows)
    if case == 'snapshot-fail':
        assert result.returncode and not any(row[0] == 'launcher' for row in rows), rows
        print('PASS', case)
        continue
    assert rows[-1][:2] == ['guard', 'verify'], (case, 'missing final verify', rows)
    assert rows[-2][:2] == ['launcher', 'shutdown'], (case, 'verify precedes shutdown', rows)
    assert (result.returncode == 0) == (case in ['success', 'retry']), (case, result.returncode, result.stderr)
    probes = [row for row in rows if row[0] == 'launcher' and len(row) > 2 and row[2].startswith('/Applications/')]
    if case in ['success', 'retry']:
        assert len(probes) == 5, (case, probes)
    if case == 'initialize-fail':
        assert not probes
        assert sum(row == ['launcher', 'shell', '/usr/bin/true'] for row in rows) == 2
    print('PASS', case)
print('PASS host guard ordering, failure propagation, five probes and startup retry')
