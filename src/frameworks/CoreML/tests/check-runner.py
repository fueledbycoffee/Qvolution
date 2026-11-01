#!/usr/bin/env python3
"""Execute runner lifecycle with fake launchers/guards; never boot a container."""
from pathlib import Path
import os, subprocess, sys, uuid
script = Path(sys.argv[1]).resolve()
kind = sys.argv[2]
out = Path(sys.argv[3]).resolve(); out.mkdir(parents=True, exist_ok=True)
for scenario in ('init-fails', 'guard-fails'):
    root = out/scenario; root.mkdir(parents=True, exist_ok=True)
    lane = root/'parallel'/('imp-'+kind)
    (root/'dashboard').mkdir(exist_ok=True)
    (root/'bin').mkdir(exist_ok=True)
    for d in ('evidence', 'evidence/guest', 'build/payloads', 'test-build', 'candidate/payloads', 'candidate/tests', 'stage/usr/local/libexec/darling', 'build/src/startup', 'build/src/frameworks/CoreML', 'build/src/frameworks/Vision'):
        (lane/d).mkdir(parents=True, exist_ok=True)
    (lane/'evidence/install.json').write_text('{}')
    (lane/'frameworks.json').write_text('{}')
    events = root/'events'
    events.write_text('')
    launcher = '''#!/usr/bin/env bash
printf '%s\n' "$1" >> "$EVENTS"
if [[ $1 == shell && $2 == /usr/bin/true && $SCENARIO == init-fails ]]; then exit 9; fi
if [[ ${3-} == --vision-coreml-subset && ${4-} == */baseline/* ]]; then echo 'missing VNBarcodeSymbology'; exit 1; fi
exit 0
'''
    for target in ('build/payloads/Launcher', 'build/src/startup/darling'):
        p = lane/target; p.write_text(launcher); p.chmod(0o755)
    for target in ('media-import-load', 'clean-rect', 'speech', 'pattern', 'gamepad'):
        p=lane/'test-build'/target; p.write_text('fixture'); p.chmod(0o755)
    for target in ('load-exports','coreml-behavior'):
        p=lane/'candidate/tests'/target; p.write_text('fixture'); p.chmod(0o755)
    for target in ('Vision','CoreML'):
        (lane/'candidate/payloads'/target).write_text('fixture')
        (lane/'build/src/frameworks'/target/target).write_text('fixture')
    for name in ('Vision','CoreML'):
        p=root/'parallel/stage41/build/src/frameworks'/('Vision/Vision' if name=='Vision' else 'load-only/CoreML'); p.parent.mkdir(parents=True,exist_ok=True); p.write_text('fixture')
    p=lane/'build/src/frameworks/CoreML/imp-vision-load-exports'; p.write_text('fixture'); p.chmod(0o755)
    p=root/'bin/git'; p.write_text('#!/bin/sh\nexit 0\n'); p.chmod(0o755)
    p=root/'dashboard/qv-host-guard'
    p.write_text('''#!/usr/bin/env bash
printf '%s\n' "$1" >> "$EVENTS"
if [[ $1 == snapshot ]]; then printf fixture > "$2"; fi
if [[ $1 == verify && $SCENARIO == guard-fails ]]; then exit 3; fi
exit 0
'''); p.chmod(0o755)
    source = script.read_text().replace('W=/home/sean/Documents/Codex/Projects/Qvolution-work', 'W='+str(root))
    source = source.replace('export DPREFIX=$W/p/im-media-$(date +%s)-$$', 'export DPREFIX=/tmp/qv-runner-fixture-$$').replace('export DPREFIX=$W/p/im-vision-$(date +%s)-$$', 'export DPREFIX=/tmp/qv-runner-fixture-$$')
    source = source.replace('/tmp/qv-runner-fixture-$$', '/tmp/qv-runner-fixture-'+uuid.uuid4().hex[:12])
    source = source.replace('D=$W/parallel/imp-media/build/payloads/Launcher', 'D=$L/build/payloads/Launcher')
    probe = root/'runner.sh'; probe.write_text(source)
    env=dict(os.environ,PATH=str(root/'bin')+':'+os.environ['PATH'],EVENTS=str(events),SCENARIO=scenario)
    r=subprocess.run(['bash', str(probe), '--slot' if kind=='media' else '--guest'], env=env, text=True, capture_output=True)
    (root/'output.log').write_text(r.stdout+r.stderr)
    seen=events.read_text().splitlines()
    assert seen and seen[0]=='snapshot', (scenario, 'no pre-boot guard', seen, r.stderr)
    assert seen[-2:]==['shutdown','verify'], (scenario, 'cleanup/verify missing', seen)
    assert r.returncode != 0, (scenario, 'failure was swallowed', seen)
    if scenario=='init-fails': assert seen.count('shell')==2, seen
    else: assert r.returncode==3, ('guard failure not propagated', r.returncode, r.stderr)
    print('PASS',kind,scenario,'status',r.returncode,seen)
