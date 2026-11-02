#!/usr/bin/env python3
"""Exercise the external handoff through the real checks, without launching a guest."""
from pathlib import Path
import csv
import json
import subprocess
import tempfile

lane = Path(__file__).resolve().parent
out = Path(tempfile.mkdtemp(prefix='r3b-validation-', dir=lane / 'evidence'))
print(f'Validation evidence: {out}', flush=True)
with (out / 'dry-run.log').open('w') as log:
    result = subprocess.run(['bash', str(lane / 'run-external.sh'), '--test-only-before-container'],
                            stdout=log, stderr=subprocess.STDOUT)
assert result.returncode == 0, out / 'dry-run.log'
evidence = Path((lane / 'external-evidence.txt').read_text().strip())
(out / 'external-evidence.txt').write_text(str(evidence) + '\n')
assert len(json.loads((evidence / 'input-sha256.json').read_text())) == 10
assert (evidence / 'provenance.json').read_bytes() == (lane / 'build/provenance.json').read_bytes()
assert len(list((evidence / 'map-check').glob('*.otool-l.txt'))) == 5
assert (evidence / 'preflight-host-folders.verify').read_text() == ''
assert 'TEST ONLY: launch boundary:' in (evidence / 'preflight-launch.txt').read_text()
with (evidence / 'guest-plan.tsv').open() as stream:
    plan = list(csv.reader(stream, delimiter='\t'))
assert len(plan) == 84 and len({row[0] for row in plan}) == 84
counts = {binary: sum(row[1] == binary for row in plan) for binary in {row[1] for row in plan}}
assert counts == {'before-custom-i386': 20, 'after-custom-i386': 20,
                  'before-generated-i386': 20, 'after-generated-i386': 20,
                  'installed-i386': 2, 'installed-x86_64': 2}
assert not (evidence / 'guest-prefix.txt').exists()
assert not (evidence / 'guest-results.tsv').exists()
assert not (evidence / 'native-i386-results.txt').exists()
prepared = Path((lane / 'prepared-evidence.txt').read_text().strip())
# Corrupt only a new result manifest; retain all prepared inputs and failed evidence.
bad = out / 'changed-manifest'
bad.mkdir()
hashes = json.loads((evidence / 'input-sha256.json').read_text())
hashes[str(prepared / 'native-i386/probe-32')] = '0' * 64
(bad / 'input-sha256.json').write_text(json.dumps(hashes))
(bad / 'provenance.json').write_bytes((evidence / 'provenance.json').read_bytes())
result = subprocess.run(['python3', str(lane / 'verify-inputs.py'), '--verify', str(prepared),
                         '--output', str(bad)], capture_output=True, text=True)
(out / 'changed-manifest.log').write_text(result.stdout + result.stderr)
assert result.returncode != 0 and 'built input changed' in result.stderr
result = subprocess.run(['bash', str(lane / 'run-guest.sh'), '--slot', str(prepared)],
                        capture_output=True, text=True)
(out / 'missing-output.log').write_text(result.stdout + result.stderr)
assert result.returncode != 0 and 'fresh external evidence directory' in result.stderr
(out / 'result.txt').write_text('PASS: external preflight, ten hashes, five maps, 84 planned cases, host guard; changed hash and missing output rejected. No native i386 or guest execution.\n')
print((out / 'result.txt').read_text(), end='')
