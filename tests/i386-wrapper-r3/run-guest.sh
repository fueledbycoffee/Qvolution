#!/usr/bin/env bash
# EXTERNAL ONLY. Expected old-wrapper failures must reach the fixture diagnostic.
set -euo pipefail
ulimit -c 0
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-wrappers-r3
export PATH=$W/dashboard/bin:$PATH LC_ALL=C
if [[ ${1-} != --slot ]]; then exec "$W/dashboard/qv-slot" guest bash "$0" --slot "$@"; fi
shift
test_only=0
if [[ ${1-} == --test-only-before-container ]]; then test_only=1; shift; fi
E=${1:?prepared input directory}
R=${2:?fresh external evidence directory}
[[ $# == 2 && -d $E && -d $R && $E != "$R" && ! -e $R/guest-results.tsv ]]
launcher=$W/parallel/i386-guest/build/darling
[[ -x $launcher && ! -u $launcher && ! -g $launcher ]]
python3 "$L/verify-inputs.py" --verify "$E" --output "$R"
python3 "$W/parallel/i386-wrappers-r2/check-fixture-maps.py" --out "$R/map-check" "$E/guest-build/"*-i386
# Resolve every fixture and argument before any container launch.
{
    for round in {1..20}; do
        printf 'before-custom-thread_selfusage-%s\tbefore-custom-i386\t6\n' "$round"
        printf 'after-custom-thread_selfusage-%s\tafter-custom-i386\t6\n' "$round"
        printf 'before-generated-set_ldt-%s\tbefore-generated-i386\t12\n' "$round"
        printf 'after-generated-thread_selfusage-%s\tafter-generated-i386\t6\n' "$round"
    done
    for round in 1 2; do
        for arch in i386 x86_64; do printf 'installed-%s-%s\tinstalled-%s\t\n' "$arch" "$round" "$arch"; done
    done
} > "$R/guest-plan.tsv"
while IFS=$'\t' read -r name binary argument; do
    [[ -x $E/guest-build/$binary ]]
    [[ $argument == 6 || $argument == 12 || ( $binary == installed-* && -z $argument ) ]]
done < "$R/guest-plan.tsv"
stage=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["stage"])' "$E/stage-farm.json")
export DARLING_NONROOT=1 DARLING_INSTALL_PREFIX=$stage/usr/local
export DPREFIX=$W/p/iw3-$(python3 -c 'import secrets; print(secrets.token_hex(3))')
[[ ${#DPREFIX} -le 83 && ! -e $DPREFIX ]]
for variable in ${!DYLD_@}; do unset "$variable"; done
unset DARLING_I386_STARTUP
if [[ $test_only == 0 ]]; then printf '%s\n' "$DPREFIX" > "$R/guest-prefix.txt"; fi
guard=$R/host-folders.before
if [[ $test_only == 1 ]]; then guard=$R/preflight-host-folders.before; fi
"$W/dashboard/qv-host-guard" snapshot "$guard"
# TEST ONLY, OFF by default: no prefix creation, launcher or shutdown call.
if [[ $test_only == 1 ]]; then
    "$W/dashboard/qv-host-guard" verify "$guard" > "$R/preflight-host-folders.verify" 2>&1
    python3 "$L/farm.py" verify "$E" > "$R/preflight-stage-verify.log" 2>&1
    printf 'TEST ONLY: launch boundary: DARLING_NONROOT=%q DARLING_INSTALL_PREFIX=%q DPREFIX=%q %q shell /usr/bin/true\n' \
        "$DARLING_NONROOT" "$DARLING_INSTALL_PREFIX" "$DPREFIX" "$launcher" | tee "$R/preflight-launch.txt"
    exit 0
fi
cleanup() {
    local rc=$? shutdown_rc=0 guard_rc=0 farm_rc=0
    trap - EXIT
    DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher" > "$R/shutdown.log" 2>&1 || shutdown_rc=$?
    "$W/dashboard/qv-host-guard" verify "$R/host-folders.before" > "$R/host-folders.verify" 2>&1 || guard_rc=$?
    python3 "$L/farm.py" verify "$E" > "$R/stage-final-verify.log" 2>&1 || farm_rc=$?
    [[ $shutdown_rc == 0 ]] || rc=$shutdown_rc
    [[ $guard_rc == 0 ]] || rc=$guard_rc
    [[ $farm_rc == 0 ]] || rc=$farm_rc
    exit "$rc"
}
trap cleanup EXIT
if ! "$launcher" shell /usr/bin/true > "$R/init-1.log" 2>&1; then
    grep -Iq 'Timed out waiting for shellspawn in container' "$R/init-1.log" || exit 1
    DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
    "$launcher" shell /usr/bin/true > "$R/init-2.log" 2>&1
fi
DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
# Initialize and shut down before copying anything into the fresh prefix.
cp -a "$E/guest-build/before-custom-i386" "$E/guest-build/after-custom-i386" \
    "$E/guest-build/before-generated-i386" "$E/guest-build/after-generated-i386" \
    "$E/guest-build/installed-i386" "$E/guest-build/installed-x86_64" "$DPREFIX/Applications/"
printf 'case\tstatus\n' > "$R/guest-results.tsv"
run_case() {
    local name=$1 binary=$2 status=0
    shift 2
    "$launcher" shell "/Applications/$binary" "$@" > "$R/$name.stdout" 2> "$R/$name.stderr" || status=$?
    if [[ $status != 0 ]] && grep -Iq 'Timed out waiting for shellspawn in container' "$R/$name.stderr" &&
            ! grep -Iq 'Failed to allocate stack' "$R/$name.stderr"; then
        cp -a "$R/$name.stdout" "$R/$name.attempt1.stdout"
        cp -a "$R/$name.stderr" "$R/$name.attempt1.stderr"
        DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
        status=0
        "$launcher" shell "/Applications/$binary" "$@" > "$R/$name.stdout" 2> "$R/$name.stderr" || status=$?
    fi
    printf '%s\t%s\n' "$name" "$status" >> "$R/guest-results.tsv"
}
while IFS=$'\t' read -r name binary argument; do
    if [[ -n $argument ]]; then run_case "$name" "$binary" "$argument" < /dev/null;
    else run_case "$name" "$binary" < /dev/null; fi
done < "$R/guest-plan.tsv"
python3 - "$R" <<'PY'
import csv, sys
from pathlib import Path
r = Path(sys.argv[1])
rows = list(csv.DictReader((r / 'guest-results.tsv').open(), delimiter='\t'))
assert len(rows) == 84
counts = {group: 0 for group in ('before-custom', 'after-custom', 'before-generated', 'after-generated', 'installed')}
for row in rows:
    name, code = row['case'], int(row['status'])
    stdout, stderr = [(r / (name + suffix)).read_text() for suffix in ('.stdout', '.stderr')]
    assert 'Failed to allocate stack' not in stderr, (name, stderr)
    category = '-'.join(name.split('-')[:2]) if not name.startswith('installed-') else 'installed'
    if category.startswith('before-'):
        assert code == 1 and 'expected-low=' in stderr, (name, code, stderr)
    else:
        assert code == 0 and 'PASS' in stdout, (name, code, stdout, stderr)
    if category == 'installed':
        assert 'installed pipe descriptor/data success PASS' in stdout
        assert 'exhausted=1 direct=-1 errno=24 wrapper=-1' in stdout
    counts[category] += 1
assert counts == {'before-custom': 20, 'after-custom': 20, 'before-generated': 20, 'after-generated': 20, 'installed': 4}, counts
(r / 'pass-counts.txt').write_text('before-custom-thread_selfusage: 20/20 expected wrapper failures\nafter-custom-thread_selfusage: 20/20 success\nbefore-generated-set_ldt: 20/20 expected wrapper failures\nafter-generated-thread_selfusage: 20/20 success\ninstalled APIs: 4/4 success\n')
print((r / 'pass-counts.txt').read_text(), end='')
PY
