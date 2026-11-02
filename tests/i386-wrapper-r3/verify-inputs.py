#!/usr/bin/env python3
"""Bind isolated committed runners and prepared inputs to fresh external evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
mode = p.add_mutually_exclusive_group(required=True)
mode.add_argument('--record', type=Path)
mode.add_argument('--verify', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
lane = Path(__file__).resolve().parent
source = lane / 'source'
assert not subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip(), source
for snapshot in (source / 'tests/i386-wrapper-r3').iterdir():
    assert snapshot.is_file() and not snapshot.is_symlink(), snapshot
    assert snapshot.read_bytes() == (lane / snapshot.name).read_bytes(), snapshot
assert (source / 'tests/mldr-stack-allocation.py').read_bytes() == (lane / 'check-stack.py').read_bytes()
subprocess.run(['python3', str(lane / 'build.py'), 'verify'], check=True)
prepared = (a.record or a.verify).resolve(strict=True)
out = a.output.resolve(strict=True)
assert prepared != out, 'keep prepared inputs separate from external results'
launcher = lane.parent / 'i386-guest/build/darling'
files = [prepared / 'native-i386/probe-32', prepared / 'native-i386-before/probe-32',
         prepared / 'observe-native-stack', launcher]
files += [prepared / 'guest-build' / name for name in (
    'before-custom-i386', 'before-generated-i386', 'after-custom-i386',
    'after-generated-i386', 'installed-i386', 'installed-x86_64')]

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

# The original preparation pinned eight inputs; retain that independent check.
legacy = json.loads((prepared / 'fixture-sha256.json').read_text())
assert set(legacy) == set(map(str, files[:4] + [files[i] for i in (5, 7, 8, 9)])), 'unexpected prepared input set'
for name, expected in legacy.items():
    assert digest(Path(name)) == expected, name
hashes = {str(path): digest(path) for path in files}
manifest = out / 'input-sha256.json'
provenance = out / 'provenance.json'
if a.record:
    assert not manifest.exists() and not provenance.exists(), 'use fresh external evidence'
    manifest.write_text(json.dumps(hashes, indent=2) + '\n')
    provenance.write_bytes((lane / 'build/provenance.json').read_bytes())
else:
    assert hashes == json.loads(manifest.read_text()), 'built input changed'
    assert provenance.read_bytes() == (lane / 'build/provenance.json').read_bytes(), 'loader provenance changed'
subprocess.run(['python3', str(lane / 'farm.py'), 'verify', str(prepared)], check=True)
print('PASS: isolated committed snapshots, ten input hashes, loader provenance and farm')
