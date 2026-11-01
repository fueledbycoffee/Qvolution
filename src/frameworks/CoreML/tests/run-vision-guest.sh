#!/usr/bin/env bash
# External container-capable host only; reuses the existing media candidate stage.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/imp-vision
if [[ ${1-} != --guest ]]; then exec "$W/dashboard/qv-slot" guest "$0" --guest; fi
for name in ${!DYLD_@}; do unset "$name"; done
export DARLING_NONROOT=1
export DARLING_INSTALL_PREFIX=$W/parallel/imp-media/stage/usr/local
export DPREFIX=$W/p/im-vision-$(date +%s)-$$
[[ ${#DPREFIX} -le 83 && ! -e $DPREFIX ]]
D=$W/parallel/imp-media/build/payloads/Launcher
[[ -x $D && -f $L/evidence/install.json ]]
[[ -x $L/candidate/tests/load-exports && -x $L/candidate/tests/coreml-behavior ]]
test -z "$(git -C "$L/superproject" status --porcelain --ignore-submodules=all)"
mkdir -p "$L/evidence/guest"
guard=$W/dashboard/qv-host-guard
snapshot=$L/evidence/guest/host-before-$(date +%s)-$$.txt
"$guard" snapshot "$snapshot"
finish() {
  rc=$?
  trap - EXIT
  "$D" shutdown >> "$L/evidence/guest/shutdown.log" 2>&1 || rc=1
  "$guard" verify "$snapshot" >> "$L/evidence/guest/host-guard.log" 2>&1 || rc=$?
  printf '%s\n' "$rc" > "$L/evidence/guest/exit-status.txt"
  exit "$rc"
}
trap finish EXIT
# Cleanup covers both initialization attempts. Only the launcher creates a prefix.
if ! "$D" shell /usr/bin/true > "$L/evidence/guest/init-1.log" 2>&1; then
  "$D" shutdown >> "$L/evidence/guest/shutdown.log" 2>&1 || true
  "$D" shell /usr/bin/true > "$L/evidence/guest/init-2.log" 2>&1
fi
"$D" shutdown >> "$L/evidence/guest/shutdown.log" 2>&1
mkdir -p "$DPREFIX/Applications/imp-vision"
cp -a "$L/candidate/tests/load-exports" "$L/candidate/tests/coreml-behavior" "$L/candidate/payloads/Vision" "$L/candidate/payloads/CoreML" "$DPREFIX/Applications/imp-vision/"
"$D" shell /Applications/imp-vision/load-exports --vision-coreml-subset /Applications/imp-vision/Vision /Applications/imp-vision/CoreML > "$L/evidence/guest/exports.log" 2>&1
"$D" shell /Applications/imp-vision/coreml-behavior > "$L/evidence/guest/behavior.log" 2>&1
printf 'Vision/CoreML subset passed; 19 VisionKit Swift imports remain OPEN.\n' > "$L/evidence/guest/result.txt"
