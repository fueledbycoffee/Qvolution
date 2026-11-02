#!/usr/bin/env bash
# EXTERNAL EXECUTOR ONLY: native ia32 and Darling container access required.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-guest-r3
S=$W/parallel/i386-guest/superproject
export PATH=$W/dashboard/bin:$PATH
[[ $# == 0 ]]
python3 - "$S" "$L" <<'PY'
from pathlib import Path
import subprocess, sys
s, lane = map(Path, sys.argv[1:])
for repo in (s, s / 'src/external/xnu', s / 'src/external/dyld'):
    assert not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip(), str(repo) + ' is dirty'
for source in (s / 'tests/i386-startup-r3').iterdir():
    if source.is_file():
        assert source.read_bytes() == (lane / source.name).read_bytes(), 'r3 script differs: ' + source.name
PY
E=$(mktemp -d "$L/evidence/native-review-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
finish() {
    local rc=$?
    trap - EXIT
    printf 'Native evidence: %s\n' "$E"
    if [[ -f $E/guest-review.log ]]; then cat "$E/guest-review.log"; fi
    "$W/dashboard/qv-status" i386-guest-r3 --state review --evidence "$E" --msg "External r3 review exited $rc"
    exit "$rc"
}
trap finish EXIT
"$W/dashboard/qv-status" i386-guest-r3 --phase 'external native ia32 TSD and guest hello review'
"$L/build-native.sh" > "$E/build-native.log" 2>&1
python3 "$L/check-native.py" --all > "$E/native-tsd.json"
"$W/dashboard/qv-slot" build python3 "$S/src/external/xnu/darling/tests/i386-tsd-descriptor.py" --out "$E/descriptor" > "$E/descriptor.log" 2>&1
# Shared runner creates a fresh guarded rootless farm; it never boots here.
bash "$W/parallel/i386-guest/run-review.sh" > "$E/guest-review.log" 2>&1
