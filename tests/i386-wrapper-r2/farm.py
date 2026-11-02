#!/usr/bin/env python3
"""Reuse the inherited hardlink farm; update private candidate files only."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys

lane = Path(__file__).resolve().parent
old_lane = lane.parent / 'i386-wrappers'
old_evidence = old_lane / 'evidence/review-20261101T212942Z-z0PIBs'
action = sys.argv[1]
evidence = Path(sys.argv[2]).resolve(strict=True)
assert action in ('prepare', 'verify')

def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def verify():
    subprocess.run(['python3', str(old_lane / 'farm.py'), 'verify', str(evidence)], check=True)
    images = json.loads((evidence / 'installed-candidate.json').read_text())
    for name, record in images.items():
        assert digest(record['source']) == record['sha256'], name
    print('PASS: inherited private runtime images exactly match current committed build')

if action == 'prepare':
    subprocess.run(['python3', str(lane.parent / 'i386-guest/build-provenance.py'), '--verify'], check=True)
    subprocess.run(['python3', str(old_lane / 'farm.py'), 'verify', str(old_evidence)], check=True)
    for name in ('stage-farm.json', 'installed-candidate.json'):
        shutil.copyfile(old_evidence / name, evidence / name)
    shutil.copyfile(evidence / 'installed-candidate.json', evidence / 'installed-candidate.before.json')
    stage = Path(json.loads((evidence / 'stage-farm.json').read_text())['stage'])
    images = json.loads((evidence / 'installed-candidate.json').read_text())
    for name, record in images.items():
        target = stage / name
        assert target.stat().st_nlink == 1 and not target.is_symlink(), target
        current = digest(record['source'])
        if current != record['sha256']:
            shutil.copyfile(record['source'], target)
            record['sha256'] = current
            record['updated_in_round2'] = True
    (evidence / 'installed-candidate.json').write_text(json.dumps(images, indent=2) + '\n')
verify()
