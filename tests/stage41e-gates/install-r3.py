#!/usr/bin/env python3
"""Replace three lane-owned payloads while preserving their original hardlinks."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

L = Path(__file__).resolve().parents[3]
S = L / 'superproject'
STAGE = L / 'stage'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


if __name__ == '__main__':
    assert not subprocess.check_output(['git', '-C', str(S), 'status', '--porcelain'])
    manifest = json.loads((STAGE / 'qvolution-runtime-source.json').read_text())
    assert not (L / 'runtime-overrides-r3.json').exists(), 'install already recorded'
    for rel, digest in manifest['fingerprints'].items():
        assert sha(STAGE / rel) == digest, rel
    replacements = []
    for name in ('Automator', 'AVFAudio', 'MediaPlayer'):
        rel = f'usr/local/libexec/darling/System/Library/Frameworks/{name}.framework/{name}'
        dest = (STAGE / rel).resolve(strict=True)
        assert dest.is_relative_to(STAGE) and dest.is_file()
        backup = L / f'evidence/r3/baseline-{name}'
        assert not backup.exists()
        source = L / 'build-r3' / name
        assert source.is_file()
        replacements.append((name, rel, source, dest, backup, sha(dest)))
    overrides = {}
    for name, rel, source, dest, backup, original in replacements:
        # Preserve the old file and all shared hardlinks. The new copy gets its
        # own inode; copying over the old inode would modify Stage41e itself.
        dest.rename(backup)
        shutil.copy2(source, dest)
        assert dest.stat().st_nlink == 1 and sha(backup) == original
        overrides[rel] = dict(sha256=sha(dest), baseline_sha256=original, baseline_backup=str(backup))
        print('installed', name, 'with original preserved at', backup, flush=True)
    (L / 'runtime-overrides-r3.json').write_text(json.dumps(overrides, indent=2) + '\n')
