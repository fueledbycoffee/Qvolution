#!/usr/bin/env python3
"""Install private candidate images into an evidence-backed hardlink farm."""
from pathlib import Path
import subprocess, sys
from stage_farm import replace
L = Path('/home/sean/Documents/Codex/Projects/Qvolution-work/parallel/i386-guest')
evidence = Path(sys.argv[1]).resolve(strict=True)
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
replace(evidence, files)
print('Installed 5 private candidate files; source manifest and image hashes unchanged')
