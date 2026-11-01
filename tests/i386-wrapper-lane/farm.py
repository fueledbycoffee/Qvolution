#!/usr/bin/env python3
"""Use the handed-over rootless farm implementation with this lane's directory."""
from pathlib import Path
import subprocess
import sys

lane = Path(__file__).resolve().parent
shared_lane = lane.parent / 'i386-guest'
sys.path.insert(0, str(shared_lane))
import stage_farm

action, directory = sys.argv[1:]
evidence = Path(directory).resolve(strict=True)
if action == 'prepare':
    subprocess.run(['python3', str(shared_lane / 'build-provenance.py'), '--verify'], check=True)
    dry = subprocess.check_output(['ninja', '-C', str(shared_lane / 'build'), '-n'], text=True)
    assert dry.rstrip().endswith('ninja: no work to do.'), dry
    stage_farm.create(evidence, lane=lane)
    images = {
      'usr/local/libexec/darling/bin/mldr': shared_lane / 'build/mldr',
      'usr/local/libexec/darling/usr/libexec/darling/mldr': shared_lane / 'build/mldr',
      'usr/local/libexec/darling/usr/libexec/darling/mldr32': shared_lane / 'build/mldr32',
      'usr/local/libexec/darling/usr/lib/dyld': shared_lane / 'build/src/external/dyld/dyld',
      'usr/local/libexec/darling/usr/lib/system/libsystem_kernel.dylib': shared_lane / 'build/src/external/xnu/darling/src/libsystem_kernel/libsystem_kernel.dylib',
    }
    stage_farm.replace(evidence, images, lane=lane)
elif action == 'verify':
    stage_farm.verify(evidence, (evidence / 'installed-candidate.json').exists(), lane=lane)
else:
    raise SystemExit('usage: farm.py prepare|verify EVIDENCE')
print('PASS: source Stage41d manifest/images unchanged; private candidate images verified')
