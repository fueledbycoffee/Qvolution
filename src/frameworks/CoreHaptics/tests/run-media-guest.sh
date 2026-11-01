#!/usr/bin/env bash
# Run only on a container-capable host. Build and install are already complete.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/imp-media
if [[ ! -f "$L/evidence/install.json" ]]; then
  echo "Candidate installation is incomplete; finish verify-and-install.sh first." >&2
  exit 2
fi
if [[ ${1-} != --slot ]]; then exec "$W/dashboard/qv-slot" guest "$0" --slot; fi
# Clear every inherited DYLD_* variable without exposing its value.
for name in ${!DYLD_@}; do unset "$name"; done
export DARLING_NONROOT=1
export DARLING_INSTALL_PREFIX=$L/stage/usr/local
export DPREFIX=$W/p/im-media-$(date +%s)-$$
[[ ${#DPREFIX} -le 83 && ! -e $DPREFIX ]]
launcher=$L/build/payloads/Launcher
[[ -x $launcher && -x $L/test-build/media-import-load ]]
test -z "$(git -C "$L/superproject" status --porcelain --ignore-submodules=all)"
mkdir -p "$L/evidence/guest"
# Only this exact, newly created prefix is shut down, even after a test failure.
guard=$W/dashboard/qv-host-guard
snapshot=$L/evidence/guest/host-before-$(date +%s)-$$.txt
"$guard" snapshot "$snapshot"
finish() {
  rc=$?
  trap - EXIT
  "$launcher" shutdown >> "$L/evidence/guest/shutdown.log" 2>&1 || rc=1
  "$guard" verify "$snapshot" >> "$L/evidence/guest/host-guard.log" 2>&1 || rc=$?
  printf '%s\n' "$rc" > "$L/evidence/guest/exit-status.txt"
  exit "$rc"
}
trap finish EXIT
if ! "$launcher" shell /usr/bin/true > "$L/evidence/guest/init-1.log" 2>&1; then
  "$launcher" shutdown >> "$L/evidence/guest/shutdown.log" 2>&1 || true
  "$launcher" shell /usr/bin/true > "$L/evidence/guest/init-2.log" 2>&1
fi
"$launcher" shutdown >> "$L/evidence/guest/shutdown.log" 2>&1
# Initialize and stop first; only now copy our tests/payloads into the prefix.
mkdir -p "$DPREFIX/Applications/imp-media/payloads" "$DPREFIX/Applications/imp-media/manifests"
cp -a "$L/test-build/media-import-load" "$L/test-build/clean-rect" "$L/test-build/speech" "$L/test-build/pattern" "$L/test-build/gamepad" "$DPREFIX/Applications/imp-media/"
python3 - "$L" "$DPREFIX/Applications/imp-media" <<'PY'
from pathlib import Path
import json,shutil,sys
l,d=map(Path,sys.argv[1:]);config=json.loads((l/'frameworks.json').read_text())
for name,spec in config.items():
 shutil.copy2(l/'build/payloads'/name,d/'payloads'/name)
 shutil.copy2(l/'superproject'/spec['symbols'],d/'manifests'/name)
PY
for lib in CoreMedia VideoToolbox MediaPlayer AVFAudio AVFoundation CoreVideo AudioToolbox GameController CoreHaptics; do
  "$launcher" shell /Applications/imp-media/media-import-load "/Applications/imp-media/payloads/$lib" "/Applications/imp-media/manifests/$lib" > "$L/evidence/guest/load-$lib.log" 2>&1
done
for test in clean-rect speech pattern gamepad; do
  "$launcher" shell "/Applications/imp-media/$test" > "$L/evidence/guest/$test.log" 2>&1
done
printf 'All 9 dlopen/export checks and 4 guest behavior executables passed.\n' | tee "$L/evidence/guest/result.txt"
