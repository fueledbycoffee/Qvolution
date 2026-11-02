#!/usr/bin/env bash
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/stage41e-gates-fix
S=$W/parallel/stage41e
[[ -z ${QV_SLOT-} ]] || { echo 'Run outside all slots' >&2; exit 2; }
dry=()
mode=all
lock=$L/input-lock-r6.json
for arg in "$@"; do
  case "$arg" in
    --dry-run-test-only) dry=(--dry-run-test-only) ;;
    --inherited-diagnostic) mode=terminal-inherited-diagnostic; lock=$L/input-lock-r6-diagnostic.json ;;
    *) echo 'usage: rerun-r6.sh [--dry-run-test-only] [--inherited-diagnostic]' >&2; exit 2 ;;
  esac
done
[[ -f $lock && -d $L/stage ]] || { echo 'Missing reviewed r6 input lock/overlay' >&2; exit 2; }
evidence=$L/evidence/rerun-r6-$(date -u +%Y%m%dT%H%M%SZ)-$$
command=(python3 "$L/superproject/tests/stage41e-gates/run.py"
  --work "$W" --source "$S/superproject" --stage "$L/stage" --mode "$mode"
  --fixtures "$S/fixtures/stage41e/specific" --launcher "$L/build-r4/darling-detached"
  --stdio-control-launcher "$L/build-r4/darling-inherited"
  --patch-receipt "$L/patch-receipt-r6.json" --stub-forwarding "$L/build-r3/stub-forwarding"
  --fatal-probe "$L/build-r5/fatal-probe" --segv-probe "$L/build-r6/segv-probe"
  --terminal "$W/parallel/terminal-reacquire/apps/Applications/Utilities/Terminal.app"
  --chess "$W/parallel/stock-apps-4/bundles/chess/Applications/Chess.app"
  --input-lock "$lock" --evidence "$evidence")
echo "Evidence: $evidence"
if [[ ${#dry[@]} -gt 0 ]]; then exec "${command[@]}" "${dry[@]}"; fi
exec "$W/dashboard/qv-slot" guest "${command[@]}"
