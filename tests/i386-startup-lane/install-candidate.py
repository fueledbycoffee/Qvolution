#!/usr/bin/env python3
"""Install only into the lane's pre-mounted Stage41d overlay; never copy a stage."""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, sys
L = Path('/home/sean/Documents/Codex/Projects/Qvolution-work/parallel/i386-guest')
W = L.parents[1]
stage = L / 'stage'
base = W / 'stage41d'
assert subprocess.check_output(['findmnt', '-n', '--mountpoint', str(stage), '-o', 'FSTYPE'], text=True).strip() == 'overlay', 'mount the lane overlay first'
assert stage.resolve() == stage and stage != base
subprocess.run(['python3', str(L / 'build-provenance.py'), '--verify'], check=True)
for component in ('', '/src/external/xnu', '/src/external/dyld'):
    repo = str(L / 'superproject') + component
    assert not subprocess.check_output(['git', '-C', repo, 'status', '--porcelain'], text=True).strip(), repo + ' has uncommitted source'
dry = subprocess.check_output(['ninja', '-C', str(L / 'build'), '-n'], text=True)
assert dry.rstrip().endswith('ninja: no work to do.'), 'build is not complete'
print(dry, end='')
files = {
    'usr/local/libexec/darling/bin/mldr': L / 'build/mldr',
    'usr/local/libexec/darling/usr/libexec/darling/mldr': L / 'build/mldr',
    'usr/local/libexec/darling/usr/libexec/darling/mldr32': L / 'build/mldr32',
    'usr/local/libexec/darling/usr/lib/dyld': L / 'build/src/external/dyld/dyld',
    'usr/local/libexec/darling/usr/lib/system/libsystem_kernel.dylib': L / 'build/src/external/xnu/darling/src/libsystem_kernel/libsystem_kernel.dylib',
}
def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()
record = {}
for relative, source in files.items():
    target = stage / relative
    original = base / relative
    assert target.resolve() == target and target.is_file(), str(target)
    assert os.stat(source).st_ino != os.stat(target).st_ino
    original_hash = digest(original)
    assert digest(target) == original_hash, 'overlay is not pristine: ' + relative
    mode = target.stat().st_mode & 0o777
    shutil.copyfile(source, target)
    target.chmod(mode)
    assert digest(target) == digest(source)
    assert digest(original) == original_hash, 'base stage changed'
    record[relative] = {'source': str(source), 'sha256': digest(source), 'base_sha256': original_hash}
(L / 'evidence/installed-candidate.json').write_text(json.dumps(record, indent=2) + '\n')
print('Installed 5 candidate files in the lane overlay; base hashes unchanged')
