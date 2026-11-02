#!/usr/bin/env bash
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/stage41e-gates-fix
S=$W/parallel/stage41e
dry=()
case "${1-}" in
  '') ;;
  --dry-run-test-only) dry=(--dry-run-test-only) ;;
  *) echo 'usage: rerun-r2.sh [--dry-run-test-only]' >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || exit 2
[[ -f $L/input-lock-r2.json && -d $L/stage ]] || {
  echo 'Missing reviewed input lock or cp -al overlay; see lane HANDOVER.md' >&2
  exit 2
}
evidence=$L/evidence/rerun-r2-$(date -u +%Y%m%dT%H%M%SZ)-$$
command=(python3 "$L/superproject/tests/stage41e-gates/run.py"
  --work "$W" --source "$S/superproject" --stage "$L/stage"
  --fixtures "$S/fixtures/stage41e/specific" --launcher "$L/build-r2/darling"
  --patch-receipt "$L/patch-receipt-r2.json" --stub-forwarding "$L/build-r2/stub-forwarding"
  --terminal "$W/parallel/terminal-reacquire/apps/Applications/Utilities/Terminal.app"
  --chess "$W/parallel/stock-apps-4/bundles/chess/Applications/Chess.app"
  --input-lock "$L/input-lock-r2.json" --evidence "$evidence")
echo "Evidence: $evidence"
if [[ ${#dry[@]} -gt 0 ]]; then
  exec "${command[@]}" "${dry[@]}"
fi
# No outer slot: this is the only guest-slot acquisition. No build/install here.
exec "$W/dashboard/qv-slot" guest "${command[@]}"
