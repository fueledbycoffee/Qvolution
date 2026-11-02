#!/usr/bin/env python3
"""Reverse only d0872f92427, verifying every changed byte and its round trip."""
from pathlib import Path
import hashlib
import json
import subprocess
L = Path(__file__).resolve().parents[3]
S = L / 'superproject'
OUT = L / 'build-r4'
delta = subprocess.check_output(['git', '-C', str(S), 'diff', '-U0', 'd0872f92427^',
                                'd0872f92427', '--', 'src/startup/darling.c'], text=True)
blocks = []
for line in delta.splitlines(keepends=True):
    if line.startswith('@@'):
        blocks.append(['', ''])
    elif blocks and line.startswith(('+', '-')):
        blocks[-1][line[0] == '+'] += line[1:]
original = (S / 'src/startup/darling.c').read_text()
control = original
cursor = 0
changes = []
for before, after in blocks:
    assert after
    index = control.index(after, cursor)
    control = control[:index] + before + control[index + len(after):]
    changes.append((index, before, after))
    cursor = index + len(before)
roundtrip = control
for index, before, after in reversed(changes):
    assert roundtrip[index:index+len(before)] == before
    roundtrip = roundtrip[:index] + after + roundtrip[index+len(before):]
assert roundtrip == original
(OUT / 'darling-inherited.c').write_text(control)
(OUT / 'control-proof.json').write_text(json.dumps(dict(
    commit='d0872f92427', hunks=len(blocks), roundtrip=True,
    current_sha256=hashlib.sha256(original.encode()).hexdigest(),
    inherited_sha256=hashlib.sha256(control.encode()).hexdigest()), indent=2)+'\n')
