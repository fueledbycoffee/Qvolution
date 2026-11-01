#!/usr/bin/env python3
"""Reject stale stages/probes without starting a container or inspecting Apple code."""
import hashlib
import json
from pathlib import Path
import stat
import subprocess
import sys

lane = Path(sys.argv[1]).resolve()
metadata = json.loads((lane/'evidence/stage.json').read_text())
for key, repo in [('superproject', lane/'superproject'),
                  ('cocotron', lane/'superproject/src/external/cocotron')]:
    head = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain', '--ignore-submodules=all'], text=True).strip()
    if dirty or metadata[key] != head:
        raise SystemExit(f'Dirty source or stale stage revision: {repo}')
launcher = lane/'build/src/startup/darling'
if launcher.stat().st_mode & stat.S_ISUID:
    raise SystemExit('Requires the own-build non-setuid launcher')
frameworks = {
    'src/external/cocotron/AppKit/AppKit': 'AppKit.framework/Versions/C/AppKit',
    'src/frameworks/Carbon/Carbon': 'Carbon.framework/Versions/A/Carbon',
    'src/frameworks/Carbon/HIToolbox/HIToolbox': 'Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/HIToolbox',
    'src/frameworks/Accessibility/Accessibility': 'Accessibility.framework/Versions/A/Accessibility',
}
probes = [
    'src/external/cocotron/AppKit/appkit_import_exports',
    'src/frameworks/Carbon/carbon_import_exports',
    'src/frameworks/Accessibility/accessibility_import_exports',
    'src/external/cocotron/AppKit/appkit_import_behavior',
    'src/frameworks/Accessibility/accessibility_import_behavior',
]

def same(product, staged):
    if not staged.resolve().is_relative_to(lane):
        raise SystemExit(f'Staged alias escapes lane: {staged}')
    def digest(path):
        with path.open('rb') as stream:
            return hashlib.file_digest(stream, 'sha256').digest()
    if digest(lane/'build'/product) != digest(staged):
        raise SystemExit(f'Stale staged own build: {staged}')

for product, install in frameworks.items():
    same(product, lane/'stage/usr/local/libexec/darling/System/Library/Frameworks'/install)
for product in probes:
    same(product, lane/'probes'/Path(product).name)
print('PASS committed revisions and hashes of existing own stage/probes; no container run')
