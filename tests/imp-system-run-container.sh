#!/usr/bin/env bash
# Run only on the container-capable host, after build/install/fixtures finish.
set -euo pipefail
W=${QV_WORKSPACE:-/home/sean/Documents/Codex/Projects/Qvolution-work}
L=$W/parallel/imp-system
if [[ -f "$L/evidence/utf16-copy-pending" && ! -f "$L/evidence/utf16-copy-verified.json" ]]; then
  echo 'Run run-reviewed-validation.sh first: UTF-16 copy fix has not passed its build/native gate' >&2
  exit 1
fi
if [[ ${1-} != --guest-slot ]]; then exec "$W/dashboard/qv-slot" guest bash "$0" --guest-slot; fi
for var in ${!DYLD_@}; do unset "$var"; done
export DARLING_NONROOT=1
export DARLING_INSTALL_PREFIX="$L/stage/usr/local"
export DPREFIX="$W/p/ims-$(date +%s)-$$"
[[ ${#DPREFIX} -le 83 && ! -e "$DPREFIX" ]]
launcher="$L/build/src/startup/darling"
[[ -x "$launcher" && ! -u "$launcher" ]]
for fixture in load-exports behavior libsystem-control cfnetwork-control port-binding; do [[ -f "$L/fixtures/imp-system-$fixture" ]]; done
for component in corefoundation foundation; do
  if [[ $component == corefoundation ]]; then name=CoreFoundation; version=A; else name=Foundation; version=C; fi
  cmp "$L/build/src/external/$component/$name" "$L/stage/usr/local/libexec/darling/System/Library/Frameworks/$name.framework/Versions/$version/$name"
done
manifest=$L/superproject/tests/imp-system-guest-exports.tsv
run="$L/evidence/container-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$run"
shutdown() { "$launcher" shutdown >> "$run/shutdown.log" 2>&1; }
finish() {
  local result=$? stopped guarded
  trap - EXIT
  set +e
  shutdown; stopped=$?
  "$W/dashboard/qv-host-guard" verify "$run/home-before.tsv" > "$run/home-verify.log" 2>&1; guarded=$?
  if (( guarded != 0 )); then result=$guarded
  elif (( result == 0 && stopped != 0 )); then result=$stopped; fi
  if (( result == 0 )); then printf 'PASS container load and behavior; evidence %s\n' "$run"; fi
  exit "$result"
}
"$W/dashboard/qv-host-guard" snapshot "$run/home-before.tsv"
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP
for attempt in 1 2; do
  if "$launcher" shell /usr/bin/true > "$run/initialize-$attempt.log" 2>&1; then break; fi
  shutdown || true
  if [[ $attempt == 2 ]] || ! grep -q 'Timed out waiting for shellspawn in container' "$run/initialize-$attempt.log"; then exit 1; fi
done
shutdown
# Only populate the prefix after the launcher has initialized it.
mkdir -p "$DPREFIX/Applications/ImpSystem"
cp -a "$L/fixtures/imp-system-load-exports" "$L/fixtures/imp-system-behavior" "$L/fixtures/imp-system-libsystem-control" "$L/fixtures/imp-system-cfnetwork-control" "$L/fixtures/imp-system-port-binding" "$DPREFIX/Applications/ImpSystem/"
cp -a "$manifest" "$DPREFIX/Applications/ImpSystem/guest-exports.tsv"
run_guest() {
  local label=$1
  shift
  for attempt in 1 2; do
    if "$launcher" shell "$@" > "$run/$label-$attempt.log" 2>&1; then
      cp "$run/$label-$attempt.log" "$run/$label.log"
      return 0
    fi
    shutdown || true
    if [[ $attempt == 2 ]] || ! grep -q 'Timed out waiting for shellspawn in container' "$run/$label-$attempt.log"; then return 1; fi
  done
}
run_guest load-exports /Applications/ImpSystem/imp-system-load-exports /Applications/ImpSystem/guest-exports.tsv
run_guest port-binding /Applications/ImpSystem/imp-system-port-binding
run_guest behavior /Applications/ImpSystem/imp-system-behavior
run_guest libsystem-control /Applications/ImpSystem/imp-system-libsystem-control /usr/lib/libSystem.B.dylib
run_guest cfnetwork-control /Applications/ImpSystem/imp-system-cfnetwork-control /System/Library/Frameworks/CFNetwork.framework/Versions/A/CFNetwork
grep -q '^RESULT imp-system-load checked=32 failures=0$' "$run/load-exports.log"
grep -q '^RESULT imp-system-port-binding failures=0$' "$run/port-binding.log"
grep -q '^RESULT imp-system-behavior failures=0$' "$run/behavior.log"
