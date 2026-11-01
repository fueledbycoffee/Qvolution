#!/usr/bin/env bash
# EXTERNAL EXECUTOR ONLY. No root, mounts or system setting changes.
# --mount-overlay remains accepted for old rerun commands; it creates a hardlink farm.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-guest
export PATH=$W/dashboard/bin:$PATH
[[ $# == 0 || ( $# == 1 && $1 == --mount-overlay ) ]]
[[ $(df -BG --output=avail / | tail -1 | tr -dc '0-9') -ge 300 ]]
E=$(mktemp -d "$L/evidence/review-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
verify_source() {
    local rc=$?
    trap - EXIT
    if [[ -f $E/stage-farm.json ]]; then
        python3 "$L/stage_farm.py" verify "$E" > "$E/stage-final-verify.log" 2>&1 || rc=$?
    fi
    exit "$rc"
}
trap verify_source EXIT
cat /proc/loadavg > "$E/load-before.txt"
# Build and native checks precede every container boot; slots never nest.
"$L/build-runtime.sh" > "$E/build.log" 2>&1
python3 "$L/build-provenance.py" --verify
"$L/check-host.py" > "$E/host-ia32.json"
"$L/build/record-gate" > "$E/record-gate.log" 2>&1
ASAN_OPTIONS=detect_leaks=0 "$L/build/stack-layout" > "$E/stack-layout.log" 2>&1
"$W/dashboard/qv-slot" build python3 "$L/stage_farm.py" create "$E" > "$E/stage.txt"
"$W/dashboard/qv-status" i386-guest-r4 --phase 'external baseline hello controls'
"$L/run-guest.sh" baseline "$E"
"$W/dashboard/qv-slot" build python3 "$L/install-candidate.py" "$E" > "$E/install.log" 2>&1
"$W/dashboard/qv-status" i386-guest-r4 --phase 'external candidate startup records'
"$L/run-guest.sh" candidate "$E"
review_status=0
python3 "$L/review-results.py" "$E" || review_status=$?
python3 "$L/stage_farm.py" verify "$E" > "$E/stage-post-guest-verify.log"
cat /proc/loadavg > "$E/load-after.txt"
printf 'Evidence: %s\n' "$E"
"$W/dashboard/qv-status" i386-guest-r4 --state review --evidence "$E" --msg "External hello review exited $review_status"
# Keep prefixes, hardlink farm and failed evidence for review; never delete.
exit "$review_status"
