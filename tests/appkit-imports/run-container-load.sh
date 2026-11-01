#!/usr/bin/env bash
# Own-framework probes only. Never run in the source-only worker sandbox.
set -euo pipefail
L=$(cd "${1:?usage: run-container-load.sh LANE [--preflight|--guest]}" && pwd)
W=$(dirname "$(dirname "$L")")
runner_dir=$(cd "$(dirname "$0")" && pwd)
if [[ ${2-} != --guest ]]; then
  [[ -z ${QV_SLOT-} ]] || { echo 'Run outside any existing slot' >&2; exit 2; }
  python3 "$runner_dir/preflight.py" "$L"
  [[ ${2-} != --preflight ]] || exit 0
  exec "$W/dashboard/qv-slot" guest bash "$0" "$L" --guest
fi
[[ ${QV_SLOT-} == guest-* ]] || { echo 'Requires qv-slot guest' >&2; exit 2; }
python3 "$runner_dir/preflight.py" "$L"
for name in ${!DYLD_@}; do unset "$name"; done
export DARLING_NONROOT=1 DARLING_INSTALL_PREFIX="$L/stage/usr/local"
export DPREFIX="$W/p/ia-$RANDOM-$$"
[[ ${#DPREFIX} -le 83 && ! -e "$DPREFIX" ]]
launcher="$L/build/src/startup/darling"
run=$(mktemp -d "$L/evidence/guest-fix-XXXXXXXX")
printf '%s\n' "$DPREFIX" > "$run/prefix.txt"
# A failed snapshot must stop before the first launcher call.
"$W/dashboard/qv-host-guard" snapshot "$run/host-before.txt" > "$run/host-snapshot.log" 2>&1
cleanup() { "$launcher" shutdown >> "$run/shutdown.log" 2>&1; }
on_exit() {
  local run_rc=$? shutdown_rc=0 guard_rc=0
  trap - EXIT
  cleanup || shutdown_rc=$?
  "$W/dashboard/qv-host-guard" verify "$run/host-before.txt" > "$run/host-verify.log" 2>&1 || guard_rc=$?
  printf 'probes=%s shutdown=%s host_guard=%s\n' "$run_rc" "$shutdown_rc" "$guard_rc" > "$run/result.txt"
  printf 'Evidence: %s\n' "$run"
  if (( guard_rc != 0 )); then cat "$run/host-verify.log" >&2; exit "$guard_rc"; fi
  if (( run_rc != 0 )); then exit "$run_rc"; fi
  exit "$shutdown_rc"
}
trap on_exit EXIT
if ! "$launcher" shell /usr/bin/true > "$run/initialize-1.log" 2>&1; then
  cleanup || true
  "$launcher" shell /usr/bin/true > "$run/initialize-2.log" 2>&1
fi
"$launcher" shutdown >> "$run/shutdown.log" 2>&1
# Only the initialized and stopped prefix receives files.
mkdir -p "$DPREFIX/Applications"
cp -a "$L/probes" "$DPREFIX/Applications/ImportProbes"
if ! "$launcher" shell /usr/bin/true > "$run/restart-1.log" 2>&1; then
  cleanup || true
  "$launcher" shell /usr/bin/true > "$run/restart-2.log" 2>&1
fi
for spec in \
  'appkit_import_exports AppKit.framework/Versions/C/AppKit' \
  'carbon_import_exports Carbon.framework/Versions/A/Carbon' \
  'accessibility_import_exports Accessibility.framework/Versions/A/Accessibility' \
  'appkit_import_behavior AppKit.framework/Versions/C/AppKit' \
  'accessibility_import_behavior Accessibility.framework/Versions/A/Accessibility'; do
  read -r probe framework <<< "$spec"
  "$launcher" shell "/Applications/ImportProbes/$probe" "/System/Library/Frameworks/$framework" > "$run/$probe.log" 2>&1
  grep -q '^PASS ' "$run/$probe.log"
done
