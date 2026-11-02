#!/usr/bin/env python3
"""Install only into the existing lane overlay; preserve every replaced inode."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

L = Path(__file__).resolve().parents[3]
S = L/'superproject'
STAGE = L/'stage'
B = L/'build-r6'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    assert not subprocess.check_output(['git', '-C', str(S), 'status', '--porcelain'])
    assert not (L/'runtime-overrides-r6.json').exists(), 'install already recorded'
    plan = json.loads((B/'build-plan.json').read_text())
    for component, head in plan['heads'].items():
        source = L.parent/'preview-r10b'/component
        assert subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip() == head
        assert not subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'])
    for path, digest in {**plan['inputs'], **plan['commands']}.items():
        assert sha(path) == digest, path
    assert 'ninja: no work to do.' in subprocess.check_output(['ninja', '-C', str(B), '-n'], text=True)
    old = json.loads((L/'patch-receipt-r5.json').read_text())
    overrides = dict(old['runtime_overrides'])
    manifest = json.loads((STAGE/'qvolution-runtime-source.json').read_text())
    for rel, digest in manifest['fingerprints'].items():
        assert sha(STAGE/rel) == overrides.get(rel, {}).get('sha256', digest), rel
    replacements = []
    for name, install, baseline in (
        ('libsystem_kernel.dylib', 'usr/lib/system/libsystem_kernel.dylib', 'src/external/xnu/darling/src/libsystem_kernel/libsystem_kernel.dylib'),
        ('dyld', 'usr/lib/dyld', 'src/external/dyld/dyld')):
        rel = 'usr/local/libexec/darling/'+install
        dest = (STAGE/rel).resolve(strict=True)
        assert dest.is_relative_to(STAGE) and dest.is_file()
        backup = L/'evidence/r6'/('baseline-'+name)
        assert not backup.exists()
        assert sha(dest) == overrides[rel]['sha256'], name+' prior payload differs'
        info = subprocess.check_output([str(L.parent/'stage41e/build/src/external/cctools-port/cctools/misc/lipo'),
                                       '-info', str(B/name)], text=True)
        assert 'i386' in info and 'x86_64' in info, info
        replacements.append((rel, B/name, dest, backup, sha(dest)))
    for rel, source, dest, backup, original in replacements:
        dest.rename(backup)
        shutil.copy2(source, dest)
        assert dest.stat().st_nlink == 1 and sha(backup) == original
        overrides[rel] = {**overrides[rel], 'sha256': sha(dest),
                          'previous_sha256': original, 'previous_backup': str(backup)}
        print('installed', rel, 'original preserved', backup, flush=True)
    (L/'runtime-overrides-r6.json').write_text(json.dumps(overrides, indent=2)+'\n')


if __name__ == '__main__':
    main()
