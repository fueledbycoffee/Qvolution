#!/usr/bin/env bash
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/stage41e-gates-fix
S=$W/parallel/stage41e
dry=()
case "${1-}" in
  '') ;;
  --dry-run-test-only) dry=(--dry-run-test-only) ;;
  *) echo 'usage: rerun.sh [--dry-run-test-only]' >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || exit 2
[[ -f $L/input-lock.json && -d $L/stage ]] || {
  echo 'Missing reviewed input lock or cp -al overlay; see lane HANDOVER.md' >&2
  exit 2
}
evidence=$L/evidence/rerun-$(date -u +%Y%m%dT%H%M%SZ)-$$
command=(python3 "$L/superproject/tests/stage41e-gates/run.py"
  --work "$W" --source "$S/superproject" --stage "$L/stage"
  --fixtures "$S/fixtures/stage41e/specific" --launcher "$S/build/src/startup/darling"
  --terminal "$W/parallel/terminal-reacquire/apps/Applications/Utilities/Terminal.app"
  --chess "$W/parallel/stock-apps-4/bundles/chess/Applications/Chess.app"
  --input-lock "$L/input-lock.json" --evidence "$evidence")
echo "Evidence: $evidence"
if [[ ${#dry[@]} -gt 0 ]]; then
  exec "${command[@]}" "${dry[@]}"
fi
# No outer slot: this is the only guest-slot acquisition. No build/install here.
exec "$W/dashboard/qv-slot" guest "${command[@]}"
