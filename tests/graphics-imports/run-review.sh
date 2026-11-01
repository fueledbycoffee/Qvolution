#!/usr/bin/env bash
# Exact baseline/candidate packet; container-capable host only. Deletes nothing.
set -euo pipefail
L=${IMP_GRAPHICS_LANE:?set IMP_GRAPHICS_LANE to the prepared lane directory}
W=$(cd -- "$L/../.." && pwd)
S="$L/superproject"
python3 - "$L" <<'PY'
from pathlib import Path
import json, subprocess, sys
lane=Path(sys.argv[1])
heads=json.loads((lane/'evidence/r3-heads.json').read_text())
for relative, expected in heads.items():
    repo=lane/relative
    assert subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()==expected, 'source head differs from tested head'
    assert not subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True).strip(), 'dirty source'
PY
if [[ ${1-} != --guest ]]; then
  grep -q '^ninja: no work to do\.$' "$L/evidence/r3-build-dry-final.log"
  test -f "$L/evidence/r3-exports/guest-load.tsv"
  restore_candidate() { "$W/dashboard/qv-slot" build python3 "$L/update-fix-stage.py" candidate; }
  trap restore_candidate EXIT
  "$W/dashboard/qv-slot" build python3 "$L/update-fix-stage.py" baseline
  "$W/dashboard/qv-slot" guest "$0" --guest baseline
  restore_candidate
  trap - EXIT
  exec "$W/dashboard/qv-slot" guest "$0" --guest candidate
fi
mode=${2:?baseline or candidate}
[[ $mode == baseline || $mode == candidate ]]
for name in ${!DYLD_@}; do unset "$name"; done
export DARLING_NONROOT=1 DARLING_INSTALL_PREFIX="$L/stage/usr/local"
export DPREFIX="$W/p/igf-$(cat /proc/sys/kernel/random/uuid | cut -c1-8)"
[[ ${#DPREFIX} -le 83 && ! -e "$DPREFIX" ]]
launcher="$L/build/src/startup/darling"
[[ -x "$launcher" && ! -u "$launcher" ]]
run="$L/evidence/fix-guest-$mode-$(date +%Y%m%dT%H%M%S)-${DPREFIX##*/}"
mkdir -p "$run"
"$W/dashboard/qv-host-guard" snapshot "$run/host-folders-before.txt"
finish() {
  local result=$? guard_result=0
  trap - EXIT
  local shutdown_result=0
  "$W/dashboard/qv-shutdown" "$launcher" > "$run/shutdown.log" 2>&1 || shutdown_result=$?
  if [[ $shutdown_result -ne 0 ]]; then
    cat "$run/shutdown.log" >&2
    result=$shutdown_result
  fi
  "$W/dashboard/qv-host-guard" verify "$run/host-folders-before.txt" > "$run/host-guard.log" 2>&1 || guard_result=$?
  if [[ $guard_result -ne 0 ]]; then
    cat "$run/host-guard.log" >&2
    result=$guard_result
  fi
  printf '%s\n' "$result" > "$run/result.exit"
  echo "mode=$mode exit=$result evidence=$run"
  exit "$result"
}
trap finish EXIT
guest_shell() {
  local label=$1 attempt result
  shift
  for attempt in 1 2; do
    if "$launcher" shell "$@" > "$run/$label-$attempt.log" 2>&1; then
      cp "$run/$label-$attempt.log" "$run/$label.log"
      return 0
    else
      result=$?
    fi
    cp "$run/$label-$attempt.log" "$run/$label.log"
    if [[ $attempt -eq 2 ]] || ! grep -q 'Timed out waiting for shellspawn in container' "$run/$label-$attempt.log"; then
      return "$result"
    fi
    "$W/dashboard/qv-shutdown" "$launcher" > "$run/$label-retry-shutdown.log" 2>&1 || return $?
  done
}
guest_shell init /usr/bin/true
"$W/dashboard/qv-shutdown" "$launcher" > "$run/initialized-shutdown.log" 2>&1
fixture="$DPREFIX/Applications/ImpGraphics"
mkdir -p "$fixture"
cp -a "$L/build/src/external/cocotron/CoreGraphics/imp_graphics_layer_test" "$fixture/layer-clip"
if [[ $mode == baseline ]]; then
  if guest_shell layer-clip /Applications/ImpGraphics/layer-clip; then
    echo 'FAIL reviewed baseline unexpectedly passed clip regression' >&2
    exit 1
  fi
  # Require the intended assertion, not a loader/startup failure.
  grep -q 'FAIL .*\[clipPhases(context) isEqualToArray:' "$run/layer-clip.log"
  grep -q '^RESULT layer-clip failures=[1-9]' "$run/layer-clip.log"
  echo 'PASS reviewed baseline fails the parent clip regression'
else
  cp -a "$L/build/tests/graphics-imports/imp_graphics_load_test" "$fixture/load-exports"
  cp -a "$L/build/tests/graphics-imports/imp_graphics_behavior_test" "$fixture/framework-behavior"
  cp -a "$L/build/src/external/cocotron/CoreGraphics/imp_graphics_swift_test" "$fixture/swift-client"
  cp -a "$L/evidence/r3-exports/guest-load.tsv" "$fixture/guest-load.tsv"
  guest_shell load-exports /Applications/ImpGraphics/load-exports /Applications/ImpGraphics/guest-load.tsv /usr/lib/swift/libswiftCoreGraphicsImports.dylib
  guest_shell framework-behavior /Applications/ImpGraphics/framework-behavior
  guest_shell layer-clip /Applications/ImpGraphics/layer-clip
  guest_shell swift-client /Applications/ImpGraphics/swift-client
  cat "$run/load-exports.log" "$run/framework-behavior.log" "$run/layer-clip.log" "$run/swift-client.log"
fi
