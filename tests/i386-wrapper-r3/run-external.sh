#!/usr/bin/env bash
# Run this command outside the sandbox. It performs no builds or installs.
set -euo pipefail
ulimit -c 0
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-wrappers-r3
export PATH=$W/dashboard/bin:$PATH
E=$(mktemp -d "$L/evidence/external-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
printf '%s\n' "$E" > "$L/external-evidence.txt"
P=$(cat "$L/prepared-evidence.txt")
python3 "$L/build.py" verify
python3 "$L/farm.py" verify "$P"
python3 - "$L" <<'PY'
from pathlib import Path
import hashlib, json, sys
lane = Path(sys.argv[1])
for source in (lane / 'source/tests/i386-wrapper-r3').iterdir():
    assert source.read_bytes() == (lane / source.name).read_bytes(), source
assert (lane / 'source/tests/mldr-stack-allocation.py').read_bytes() == (lane / 'check-stack.py').read_bytes()
prepared = Path((lane / 'prepared-evidence.txt').read_text().strip())
for path, expected in json.loads((prepared / 'fixture-sha256.json').read_text()).items():
    with Path(path).open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == expected, path
PY
"$W/dashboard/qv-status" i386-wrappers-r3 --phase 'external native i386 allocation and 84 focused guest cases'
# Compilation already happened in the sandbox. Native i386 execution is blocked there.
python3 - "$P" "$E" <<'PY'
from pathlib import Path
import subprocess, sys
prepared, out = map(Path, sys.argv[1:])
binary = prepared / 'native-i386/probe-32'
passed = 0
for occupied in (0, 1, 2):
    for limit in (0, 1, 2):
        for repeat in range(20):
            result = subprocess.run([str(binary), str(occupied), str(limit), '0'], capture_output=True, text=True)
            name = f'native32-{occupied}-{limit}-{repeat}'
            (out / (name + '.stdout')).write_text(result.stdout)
            (out / (name + '.stderr')).write_text(result.stderr)
            passed += result.returncode == 0
(out / 'native-i386-results.txt').write_text(f'{passed}/180 native i386 allocation cases passed\n')
assert passed == 180, passed
baseline = prepared / 'native-i386-before/probe-32'
red = 0
for occupied in (0, 1, 2):
    for limit in (0, 1, 2):
        result = subprocess.run([str(baseline), str(occupied), str(limit), '0'], capture_output=True, text=True)
        name = f'native32-before-{occupied}-{limit}'
        (out / (name + '.stdout')).write_text(result.stdout)
        (out / (name + '.stderr')).write_text(result.stderr)
        if occupied:
            assert result.returncode == 1 and 'Failed to allocate stack' in result.stderr and 'File exists' in result.stderr
            red += 1
        elif limit == 0:
            assert result.returncode == -6 and '== size' in result.stderr
        else:
            assert result.returncode == 0, (name, result.stderr)
assert red == 6
counts = {0: 0, 10: 0, 11: 0}
with (out / 'native-startup-overlap.log').open('w') as log:
    for i in range(1024):
        result = subprocess.run([str(prepared / 'observe-native-stack')], capture_output=True, text=True)
        assert result.returncode in counts, result.returncode
        counts[result.returncode] += 1
        if result.returncode:
            log.write(f'run {i}: {result.stdout}')
    log.write(f'1024 native ELF startups: clear={counts[0]} native-stack={counts[10]} other-vma={counts[11]}\n')
PY
for name in stage-farm.json installed-candidate.json; do cp -a "$P/$name" "$E/$name"; done
cp -a "$P/guest-build" "$E/guest-build"
cp -a "$L/build/provenance.json" "$E/provenance.json"
bash "$L/run-guest.sh" "$E"
"$W/dashboard/qv-status" i386-wrappers-r3 --state review --evidence "$E" --msg 'External native i386 180/180 and focused guest 84/84 expected results; review logs'
