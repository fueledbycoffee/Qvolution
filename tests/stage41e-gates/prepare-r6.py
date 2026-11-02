#!/usr/bin/env python3
"""Lock the committed R6 observer, runner, build and preserved overlay."""
import hashlib
import json
from pathlib import Path
import subprocess
L = Path(__file__).resolve().parents[3]
S = L/'superproject'
def sha(path):
    with Path(path).open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()
def main():
    assert not subprocess.check_output(['git','-C',str(S),'status','--porcelain'])
    old = json.loads((L/'patch-receipt-r5.json').read_text())
    plan = json.loads((L/'build-r6/build-plan.json').read_text())
    changed = {str(S/'tests/stage41e-gates'/name) for name in ('run.py','diagnostics.py','processes.py','test_contracts.py','README.md','pipes.py','test_pipes.py')}
    overrides = json.loads((L/'runtime-overrides-r6.json').read_text())
    changed.update(str((L/'stage'/rel).resolve()) for rel in overrides
                   if overrides[rel]['sha256'] != old['runtime_overrides'].get(rel,{}).get('sha256'))
    inputs = {}
    for path,digest in old['inputs'].items():
        if path not in changed: assert sha(path) == digest, path
        inputs[path] = sha(path)
    for path,digest in {**plan['inputs'],**plan['commands']}.items():
        assert sha(path) == digest, path
        inputs[path] = digest
    for name in ('build-r6.py','install-r6.py','prepare-r6.py','rerun-r6.sh',
                 'signal-origin.h','segv-probe.c','test_signal_origin.c','native-check-r6.py','CMakeLists-r6.txt'):
        path = S/'tests/stage41e-gates'/name
        inputs[str(path)] = sha(path)
    for path in [L/'build-r6/build.ninja', L/'build-r6/build-plan.json',
                 L/'rerun-r6.sh', L/'runtime-overrides-r6.json', *map(Path,plan['payloads'].values())]:
        inputs[str(path)] = sha(path)
    for item in overrides.values():
        if 'previous_backup' in item:
            assert sha(item['previous_backup']) == item['previous_sha256']
            inputs[item['previous_backup']] = item['previous_sha256']
    assert (L/'rerun-r6.sh').read_bytes() == (S/'tests/stage41e-gates/rerun-r6.sh').read_bytes()
    head = subprocess.check_output(['git','-C',str(S),'rev-parse','HEAD'],text=True).strip()
    receipt = {**old, 'commit':head, 'wrapper':'rerun-r6.sh', 'inputs':inputs,
               'runtime_overrides':overrides,
               'library_slices':'i386+x86_64 kernel and full static dyld linked with raw-syscall SIGSEGV origin; R5 libc unchanged',
               'build':'dashboard/qv-slot build ninja -C '+str(L/'build-r6')+' -j6; ninja -n idle'}
    (L/'patch-receipt-r6.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('PASS receipt',head,len(inputs),'inputs')
if __name__ == '__main__': main()
