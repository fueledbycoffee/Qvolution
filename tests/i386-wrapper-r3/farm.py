#!/usr/bin/env python3
"""Prepare an isolated cp -al farm with five private candidate images."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

lane = Path(__file__).resolve().parent
shared = lane.parent / 'i386-guest'
sys.path.insert(0, str(shared))
import stage_farm

old = lane.parent / 'i386-wrappers-r2/evidence/review-20261102T094927Z-B3q7Cu'
action, directory = sys.argv[1:]
evidence = Path(directory).resolve(strict=True)

def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

subprocess.run(['python3', str(lane / 'build.py'), 'verify'], check=True)
if action == 'prepare':
    assert not (evidence / 'stage-farm.json').exists(), 'retain existing farm evidence; use a fresh directory'
    images = json.loads((old / 'installed-candidate.json').read_text())
    files = {}
    for name, record in images.items():
        if name.endswith('/mldr') or name.endswith('/mldr32'):
            files[name] = lane / 'build' / Path(name).name
        else:
            assert digest(record['source']) == record['sha256'], record['source']
            files[name] = Path(record['source'])
    # The inherited farm changed concurrently. Keep this lane's hardlinks and
    # private images separate without copying the stage payload.
    stage_farm.create(evidence, lane=lane)
    stage_farm.replace(evidence, files, lane=lane)
elif action != 'verify':
    raise SystemExit('usage: farm.py prepare|verify EVIDENCE')
stage_farm.verify(evidence, True, lane=lane)
for record in json.loads((evidence / 'installed-candidate.json').read_text()).values():
    assert digest(record['source']) == record['sha256'], record['source']
print('PASS: isolated hardlink farm/private runtime verified; Stage41d images unchanged')
