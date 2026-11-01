#!/usr/bin/env bash
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-guest
export PATH=$W/dashboard/bin:$PATH
if [[ ${1-} != --slot ]]; then exec "$W/dashboard/qv-slot" build "$0" --slot; fi
python3 "$L/prepare-build.py"
ninja -C "$L/build" -j6
dry_run=$(ninja -C "$L/build" -n)
printf '%s\n' "$dry_run"
[[ $dry_run == *'ninja: no work to do.' ]]
python3 "$L/build-provenance.py"
