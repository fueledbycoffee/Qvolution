#!/usr/bin/env bash
# Native work is allowed here; guest-only must run outside the restricted sandbox.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-wrappers-r2
S=$W/parallel/i386-guest/superproject
X=$S/src/external/xnu
export PATH=$W/dashboard/bin:$PATH
mode=${1---native-only}
[[ $mode == --native-only || $mode == --guest-only ]]
[[ ( $mode == --guest-only && $# == 2 ) || ( $mode == --native-only && $# -le 1 ) ]]
[[ $(df -BG --output=avail / | tail -1 | tr -dc '0-9') -ge 300 ]]
python3 - "$S" "$L" <<'PY'
from pathlib import Path
import subprocess, sys
s, lane = map(Path, sys.argv[1:])
for repo in (s, s / 'src/external/xnu', s / 'src/external/dyld'):
    assert not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip(), str(repo) + ' dirty'
for path in (s / 'tests/i386-wrapper-r2').iterdir():
    assert path.read_bytes() == (lane / path.name).read_bytes(), 'runner differs: ' + path.name
PY
if [[ $mode == --guest-only ]]; then
    E=${2:?native evidence directory}
    python3 "$W/parallel/i386-guest/build-provenance.py" --verify
    cmp "$W/parallel/i386-guest/build/provenance.json" "$E/provenance.json"
    python3 "$L/farm.py" verify "$E"
    python3 "$L/check-fixture-maps.py" --out "$E/map-check-before-guest" "$E/guest-build/"*-i386
    "$W/dashboard/qv-status" i386-wrappers-r2 --phase 'External 116-case wrapper and installed pipe gate'
    exec bash "$L/run-guest.sh" "$E"
fi
E=$(mktemp -d "$L/evidence/review-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
"$W/dashboard/qv-status" i386-wrappers-r2 --phase 'Committed native controls, fixture maps, runtime build and farm verification'
status=0
python3 "$L/check-fixture-maps.py" --out "$E/old-maps" \
    "$W/parallel/i386-wrappers/evidence/review-20261101T212942Z-z0PIBs/guest-build/after-custom-i386" > "$E/maps-before.log" 2>&1 || status=$?
[[ $status == 1 ]] && grep -q AssertionError "$E/maps-before.log"
status=0
"$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-wrapper-paths.py" --ref e912fb4 --out "$E/before" > "$E/before.log" 2>&1 || status=$?
[[ $status == 1 ]]
[[ $(grep -c 'exit 1' "$E/before.log") == 28 && $(grep -c 'vfork exit -11' "$E/before.log") == 2 ]]
"$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-wrapper-paths.py" --ref dd098b5 --out "$E/pipe-baseline" > "$E/pipe-baseline.log" 2>&1
[[ $(grep -c 'exit 0' "$E/pipe-baseline.log") == 30 ]]
"$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-wrapper-paths.py" --out "$E/after" > "$E/after.log" 2>&1
[[ $(grep -c 'exit 0' "$E/after.log") == 30 ]]
"$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-syscall-errors.py" --out "$E/standard" > "$E/standard.log" 2>&1
"$W/dashboard/qv-slot" build python3 "$L/check-native-pipe.py" "$E/native-pipe" > "$E/native-pipe.log" 2>&1
bash "$W/parallel/i386-guest/build-runtime.sh" > "$E/build.log" 2>&1
"$W/dashboard/qv-slot" build python3 "$L/build-guest.py" --out "$E/guest-build" > "$E/guest-build.log" 2>&1
"$W/dashboard/qv-slot" build python3 "$L/farm.py" prepare "$E" > "$E/farm.log" 2>&1
cp -a "$W/parallel/i386-guest/build/provenance.json" "$E/provenance.json"
printf '%s\n' "$E" > "$L/native-evidence.txt"
printf 'Native evidence: %s\n' "$E"
"$W/dashboard/qv-status" i386-wrappers-r2 --state review --commit "$(git -C "$S" rev-parse HEAD)" --evidence "$E" --msg 'Native/map checks pass; installed pipe discrepancy requires external guest gate'
