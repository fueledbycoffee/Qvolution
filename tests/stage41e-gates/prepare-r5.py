#!/usr/bin/env python3
"""Hash-lock committed runner, all borrowed sources and fully linked payloads."""
import hashlib
import json
from pathlib import Path
import subprocess

L = Path(__file__).resolve().parents[3]
S = L/'superproject'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    assert not subprocess.check_output(['git', '-C', str(S), 'status', '--porcelain'])
    old = json.loads((L/'patch-receipt-r4.json').read_text())
    changed = {str(S/'tests/stage41e-gates'/name) for name in
               ('run.py', 'contracts.py', 'processes.py', 'test_contracts.py')}
    inputs = {}
    for path, digest in old['inputs'].items():
        if path not in changed:
            assert sha(path) == digest, path
        inputs[path] = sha(path)
    plan = json.loads((L/'build-r5/build-plan.json').read_text())
    inputs.update(plan['inputs'])
    inputs.update(plan['commands'])
    for component, head in plan['heads'].items():
        source = L.parent/'preview-r10b'/component
        assert subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip() == head
        assert not subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'])
    for name in ('build-r5.py', 'install-r5.py', 'prepare-r5.py', 'rerun-r5.sh', 'contracts.py',
                 'diagnostics.py', 'fatal-probe.c', 'fatal-include/QVFatal.h', 'README.md'):
        path = S/'tests/stage41e-gates'/name
        inputs[str(path)] = sha(path)
    for path in [L/'build-r5/build.ninja', L/'build-r5/build-plan.json', L/'rerun-r5.sh',
                 L/'runtime-overrides-r5.json', *map(Path, plan['payloads'].values())]:
        inputs[str(path)] = sha(path)
    assert (L/'rerun-r5.sh').read_bytes() == (S/'tests/stage41e-gates/rerun-r5.sh').read_bytes()
    for path, digest in inputs.items():
        assert sha(path) == digest, path
    head = subprocess.check_output(['git', '-C', str(S), 'rev-parse', 'HEAD'], text=True).strip()
    receipt = {**old, 'commit': head, 'wrapper': 'rerun-r5.sh', 'inputs': inputs,
               'runtime_overrides': json.loads((L/'runtime-overrides-r5.json').read_text()),
               'diagnostic_heads': plan['heads'],
               'library_slices': 'i386+x86_64 libsystem_c, libsystem_kernel, full static dyld linked; static optional diagnostics off before TLS/clock bootstrap. Other library payloads unchanged.',
               'build': 'dashboard/qv-slot build ninja -C '+str(L/'build-r5')+' -j6; ninja -n idle'}
    (L/'patch-receipt-r5.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('PASS receipt', head, len(inputs), 'inputs')


if __name__ == '__main__':
    main()
