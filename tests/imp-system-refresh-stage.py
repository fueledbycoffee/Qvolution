#!/usr/bin/env python3
"""Refresh two committed own images in the existing private stage, retaining old ones."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

lane = Path(__file__).resolve().parents[2]
assert os.environ.get('QV_SLOT', '').startswith('build-')
for relative in ['', 'src/external/corefoundation', 'src/external/foundation']:
    repo = lane / 'superproject' / relative
    assert not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip(), repo
assert (lane / 'stage').is_dir(), 'Reuse the approved private stage; never create a full copy'
backup = Path(tempfile.mkdtemp(prefix='stage-before-port-', dir=lane / 'evidence'))
for component, version in [('corefoundation', 'A'), ('foundation', 'C')]:
    name = 'CoreFoundation' if component == 'corefoundation' else 'Foundation'
    relative = Path(f'System/Library/Frameworks/{name}.framework/Versions/{version}/{name}')
    source = lane / 'build/src/external' / component / name
    target = lane / 'stage/usr/local/libexec/darling' / relative
    baseline = lane.parents[1] / 'stage41b/usr/local/libexec/darling' / relative
    assert target.is_file() and not target.is_symlink()
    assert not os.path.samefile(target, baseline), 'Refuse a baseline-shared inode'
    shutil.copy2(target, backup / name)
    shutil.copy2(source, target)
    assert source.read_bytes() == target.read_bytes()
    print('Installed own image:', target, 'prior image:', backup / name)
