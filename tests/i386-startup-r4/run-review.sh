#!/usr/bin/env bash
# EXTERNAL EXECUTOR ONLY: requires native ia32 and Darling container access.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-guest-r4
S=$W/parallel/i386-guest/superproject
X=$S/src/external/xnu
export PATH=$W/dashboard/bin:$PATH
[[ $# == 0 ]]
[[ $(df -BG --output=avail / | tail -1 | tr -dc '0-9') -ge 300 ]]
python3 - "$S" "$L" <<'PY'
from pathlib import Path
import subprocess, sys
s, lane = map(Path, sys.argv[1:])
for repo in (s, s / 'src/external/xnu', s / 'src/external/dyld'):
    assert not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip(), str(repo) + ' is dirty'
for source in (s / 'tests/i386-startup-r4').iterdir():
    if source.is_file():
        assert source.read_bytes() == (lane / source.name).read_bytes(), 'r4 script differs: ' + source.name
PY
E=$(mktemp -d "$L/evidence/review-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
finish() {
    local rc=$?
    trap - EXIT
    printf 'Round 4 evidence: %s\n' "$E"
    if [[ -f $E/guest-review.log ]]; then cat "$E/guest-review.log"; fi
    "$W/dashboard/qv-status" i386-guest-r4 --state review --evidence "$E" --msg "External r4 review exited $rc"
    exit "$rc"
}
trap finish EXIT
"$W/dashboard/qv-status" i386-guest-r4 --phase 'external i386 errno and pread offset controls'
"$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-syscall-errors.py" --out "$E/errno-before" --ref 0a062090 > "$E/errno-before.log" 2>&1 && before_rc=0 || before_rc=$?
[[ $before_rc == 1 ]] # The old header must reproduce the raw -ENOENT failure.
grep -q 'custom exit 1' "$E/errno-before.log"
grep -q 'generated exit 1' "$E/errno-before.log"
"$W/dashboard/qv-slot" build python3 "$X/darling/tests/i386-syscall-errors.py" --out "$E/errno-after" > "$E/errno-after.log" 2>&1
"$W/dashboard/qv-slot" build python3 "$X/darling/tests/pread-offsets.py" --all --out "$E/pread" > "$E/pread.log" 2>&1
# The shared runner builds committed sources, checks native IA32, makes a new
# rootless hardlink farm and runs both hello architectures, off/on, twice.
# Guest boot, home-folder guard and prefix-scoped shutdown stay in run-guest.sh.
bash "$W/parallel/i386-guest/run-review.sh" > "$E/guest-review.log" 2>&1
