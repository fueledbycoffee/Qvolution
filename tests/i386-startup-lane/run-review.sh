#!/usr/bin/env bash
# EXTERNAL EXECUTOR ONLY. --mount-overlay requires passwordless mount privileges.
# No stage copies or host kernel settings are changed; only a lane overlay is mounted.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-guest
export PATH=$W/dashboard/bin:$PATH
[[ $# == 0 || ( $# == 1 && $1 == --mount-overlay ) ]]
[[ $(df -BG --output=avail / | tail -1 | tr -dc '0-9') -ge 300 ]]
E=$L/evidence/review-$(date -u +%Y%m%dT%H%M%SZ)
mkdir -p "$E"
cat /proc/loadavg > "$E/load-before.txt"
# Build and native checks precede every container boot; slots never nest.
"$L/build-runtime.sh" > "$E/build.log" 2>&1
python3 "$L/build-provenance.py" --verify
"$L/check-host.py" > "$E/host-ia32.json"
"$L/build/record-gate" > "$E/record-gate.log" 2>&1
ASAN_OPTIONS=detect_leaks=0 "$L/build/stack-layout" > "$E/stack-layout.log" 2>&1
if [[ ${1-} == --mount-overlay ]]; then
    [[ ! -e $L/stage && ! -e $L/overlay-upper && ! -e $L/overlay-work ]]
    mkdir "$L/stage" "$L/overlay-upper" "$L/overlay-work"
    sudo -n mount -t overlay overlay -o "lowerdir=$W/stage41d,upperdir=$L/overlay-upper,workdir=$L/overlay-work" "$L/stage"
fi
[[ $(findmnt -n --mountpoint "$L/stage" -o FSTYPE) == overlay ]]
options=$(findmnt -n --mountpoint "$L/stage" -o OPTIONS)
[[ ,$options, == *,lowerdir=$W/stage41d,* && ,$options, == *,upperdir=$L/overlay-upper,* ]]
[[ -z $(find "$L/overlay-upper" -mindepth 1 -maxdepth 1 -print -quit) ]]
printf 'overlay=%s\n' "$L/stage" > "$E/stage.txt"
"$W/dashboard/qv-status" i386-guest --phase 'external baseline hello controls'
"$L/run-guest.sh" baseline "$E"
"$W/dashboard/qv-slot" build python3 "$L/install-candidate.py" > "$E/install.log" 2>&1
cp -a "$L/evidence/installed-candidate.json" "$E/installed-candidate.json"
"$W/dashboard/qv-status" i386-guest --phase 'external candidate startup records'
"$L/run-guest.sh" candidate "$E"
review_status=0
python3 "$L/review-results.py" "$E" || review_status=$?
cat /proc/loadavg > "$E/load-after.txt"
printf 'Evidence: %s\n' "$E"
"$W/dashboard/qv-status" i386-guest --state review --evidence "$E" --msg "External hello review exited $review_status"
# Keep prefixes, overlay mount, upperdir and failed evidence for review; never delete.
exit "$review_status"
