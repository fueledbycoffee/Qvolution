#!/usr/bin/env python3
from pathlib import Path
import hashlib, json, subprocess, sys
L = Path('/home/sean/Documents/Codex/Projects/Qvolution-work/parallel/i386-guest')
record = {}
for name, component in (('superproject', ''), ('xnu', '/src/external/xnu'), ('dyld', '/src/external/dyld')):
    repo = str(L / 'superproject') + component
    record[name] = {
        'commit': subprocess.check_output(['git', '-C', repo, 'rev-parse', 'HEAD'], text=True).strip(),
        'status': subprocess.check_output(['git', '-C', repo, 'status', '--porcelain'], text=True).strip(),
    }
for name in ('steps.json', 'dyld.link.rsp', 'libsystem_kernel.dylib.link.rsp'):
    record[name] = hashlib.sha256((L / 'build' / name).read_bytes()).hexdigest()
for source in (L / 'superproject/tests/i386-startup-lane').iterdir():
    if source.is_file():
        assert source.read_bytes() == (L / source.name).read_bytes(), 'lane script differs from committed snapshot: ' + source.name
path = L / 'build/provenance.json'
if '--verify' in sys.argv:
    assert record == json.loads(path.read_text()), 'build provenance changed; rebuild from committed source'
    assert all(not record[name]['status'] for name in ('superproject', 'xnu', 'dyld')), 'source is not committed'
else:
    path.write_text(json.dumps(record, indent=2) + '\n')
