#!/usr/bin/env bash
# Exact external rerun; no build or install while holding a guest slot.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-wrappers-r2
export PATH=$W/dashboard/bin:$PATH
IFS= read -r E < "$L/native-evidence.txt"
[[ -n $E && -d $E && ! -e $E/guest-results.tsv ]]
exec bash "$L/run-review.sh" --guest-only "$E"
