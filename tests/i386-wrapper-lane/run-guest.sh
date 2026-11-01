#!/usr/bin/env bash
# EXTERNAL ONLY. Each fixture process tests one wrapper, including old vfork's crash.
set -euo pipefail
ulimit -c 0
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-wrappers
export PATH=$W/dashboard/bin:$PATH
if [[ ${1-} != --slot ]]; then exec "$W/dashboard/qv-slot" guest bash "$0" --slot "$@"; fi
shift
E=${1:?native evidence directory}
launcher=$L/build/darling
[[ -x $launcher && ! -u $launcher && ! -g $launcher ]]
python3 "$L/farm.py" verify "$E"
stage=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["stage"])' "$E/stage-farm.json")
export DARLING_NONROOT=1 DARLING_INSTALL_PREFIX=$stage/usr/local
export DPREFIX=$W/p/iw-$(python3 -c 'import secrets; print(secrets.token_hex(3))')
[[ ${#DPREFIX} -le 83 && ! -e $DPREFIX ]]
for variable in ${!DYLD_@}; do unset "$variable"; done
unset DARLING_I386_STARTUP
printf '%s\n' "$DPREFIX" > "$E/guest-prefix.txt"
"$W/dashboard/qv-host-guard" snapshot "$E/host-folders.before"
cleanup() {
    local rc=$? shutdown_rc=0 guard_rc=0 farm_rc=0
    trap - EXIT
    DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher" || shutdown_rc=$?
    "$W/dashboard/qv-host-guard" verify "$E/host-folders.before" > "$E/host-folders.verify" 2>&1 || guard_rc=$?
    python3 "$L/farm.py" verify "$E" > "$E/stage-final-verify.log" 2>&1 || farm_rc=$?
    [[ $shutdown_rc == 0 ]] || rc=$shutdown_rc
    [[ $guard_rc == 0 ]] || rc=$guard_rc
    [[ $farm_rc == 0 ]] || rc=$farm_rc
    exit "$rc"
}
trap cleanup EXIT
if ! "$launcher" shell /usr/bin/true > "$E/init-1.log" 2>&1; then
    DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
    "$launcher" shell /usr/bin/true > "$E/init-2.log" 2>&1
fi
DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
# Prefix initialization and shutdown precede every copy into Applications.
cp -a "$E/guest-build/before-custom-i386" "$E/guest-build/before-generated-i386" \
    "$E/guest-build/after-custom-i386" "$E/guest-build/after-generated-i386" \
    "$E/guest-build/installed-i386" "$E/guest-build/installed-x86_64" "$DPREFIX/Applications/"
printf 'case\tstatus\n' > "$E/guest-results.tsv"
run_case() {
    local name=$1 binary=$2 status=0
    shift 2
    "$launcher" shell "/Applications/$binary" "$@" > "$E/$name.stdout" 2> "$E/$name.stderr" || status=$?
    if [[ $status != 0 ]] && grep -q 'Timed out waiting for shellspawn in container' "$E/$name.stderr"; then
        cp -a "$E/$name.stdout" "$E/$name.attempt1.stdout"
        cp -a "$E/$name.stderr" "$E/$name.attempt1.stderr"
        DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
        status=0
        "$launcher" shell "/Applications/$binary" "$@" > "$E/$name.stdout" 2> "$E/$name.stderr" || status=$?
    fi
    printf '%s\t%s\n' "$name" "$status" >> "$E/guest-results.tsv"
}
wrappers=(lseek pipe gettimeofday sigaltstack sigreturn thread_selfid thread_selfusage kdebug_trace_string fork vfork syscall get_ldt set_ldt int)
for round in 1 2; do
    for variant in before after; do
        for tag in custom generated; do
            for i in "${!wrappers[@]}"; do
                run_case "$variant-$tag-${wrappers[$i]}-$round" "$variant-$tag-i386" "$i"
            done
        done
    done
    for arch in i386 x86_64; do run_case "installed-$arch-$round" "installed-$arch"; done
done
python3 - "$E" <<'PY'
from pathlib import Path
import csv, sys
e = Path(sys.argv[1])
rows = list(csv.DictReader((e / 'guest-results.tsv').open(), delimiter='\t'))
assert len(rows) == 116, len(rows)
for row in rows:
    name, code = row['case'], int(row['status'])
    stdout = (e / (name + '.stdout')).read_text()
    stderr = (e / (name + '.stderr')).read_text()
    if name.startswith('before-'):
        if '-vfork-' in name:
            assert code == 139, (name, code)
        else:
            assert code == 1 and 'expected-low=' in stderr, (name, code, stderr)
    else:
        assert code == 0 and 'PASS' in stdout, (name, code, stdout, stderr)
print('PASS: 56 red controls, 56 fixed wrapper controls, 4 installed API/thread/fork runs')
PY
