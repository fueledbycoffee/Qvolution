#!/usr/bin/env bash
# Default performs all checks; --native-only is safe in the restricted sandbox.
# --guest-only E reuses a verified native farm without another install or stage.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-wrappers
S=$L/superproject
X=$S/src/external/xnu
export PATH=$W/dashboard/bin:$PATH
mode=${1---all}
[[ $mode == --all || $mode == --native-only || $mode == --guest-only ]]
[[ ( $mode == --guest-only && $# == 2 ) || ( $mode != --guest-only && $# -le 1 ) ]]
[[ $(df -BG --output=avail / | tail -1 | tr -dc '0-9') -ge 300 ]]
python3 - "$S" "$L" <<'PY'
from pathlib import Path
import subprocess, sys
s, lane = map(Path, sys.argv[1:])
for repo in (s, s / 'src/external/xnu', s / 'src/external/dyld'):
    assert not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip(), str(repo) + ' dirty'
for path in (s / 'tests/i386-wrapper-lane').iterdir():
    assert path.read_bytes() == (lane / path.name).read_bytes(), 'runner differs: ' + path.name
PY
if [[ $mode == --guest-only ]]; then
    E=${2:?native evidence directory}
else
    E=$(mktemp -d "$L/evidence/review-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
fi
finish() {
    local rc=$?
    trap - EXIT
    "$W/dashboard/qv-status" i386-wrappers --state review --commit "$(git -C "$S" rev-parse HEAD)" --evidence "$E" --msg "Wrapper review $mode exited $rc; guest required for acceptance"
    exit "$rc"
}
trap finish EXIT
if [[ $mode == --guest-only ]]; then
    python3 "$W/parallel/i386-guest/build-provenance.py" --verify
    python3 "$L/farm.py" verify "$E"
else
    "$W/dashboard/qv-status" i386-wrappers --phase 'native wrapper regression controls and committed runtime build'
    status=0
    "$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-wrapper-paths.py" --ref e912fb4 --out "$E/before" > "$E/before.log" 2>&1 || status=$?
    [[ $status == 1 ]]
    [[ $(grep -c 'exit 1' "$E/before.log") == 28 && $(grep -c 'vfork exit -11' "$E/before.log") == 2 ]]
    "$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-wrapper-paths.py" --out "$E/after" > "$E/after.log" 2>&1
    [[ $(grep -c 'exit 0' "$E/after.log") == 30 ]]
    "$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-syscall-errors.py" --out "$E/standard" > "$E/standard.log" 2>&1
    bash "$W/parallel/i386-guest/build-runtime.sh" > "$E/build.log" 2>&1
    "$W/dashboard/qv-slot" build python3 "$L/build-guest.py" --out "$E/guest-build" > "$E/guest-build.log" 2>&1
    "$W/dashboard/qv-slot" build python3 "$L/farm.py" prepare "$E" > "$E/install.log" 2>&1
    cp -a "$L/build/provenance.json" "$E/provenance.json"
    printf '%s\n' "$E" > "$L/native-evidence.txt"
fi
printf 'Evidence: %s\n' "$E"
if [[ $mode == --native-only ]]; then exit 0; fi
cmp "$L/build/provenance.json" "$E/provenance.json"
"$W/dashboard/qv-status" i386-wrappers --phase 'external guest wrapper errors and installed API/thread/fork controls'
bash "$L/run-guest.sh" "$E"
