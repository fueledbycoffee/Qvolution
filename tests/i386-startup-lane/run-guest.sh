#!/usr/bin/env bash
# Run only our hello programs. No timeout, process-name signals or Apple apps.
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-guest
export PATH=$W/dashboard/bin:$PATH
if [[ ${1-} != --slot ]]; then exec "$W/dashboard/qv-slot" guest "$0" --slot "$@"; fi
shift
kind=${1:?baseline or candidate}
E=${2:?run evidence directory}
[[ $kind == baseline || $kind == candidate ]]
launcher=$L/build/darling
[[ -x $launcher && ! -u $launcher && ! -g $launcher ]]
export DARLING_NONROOT=1
if [[ $kind == baseline ]]; then export DARLING_INSTALL_PREFIX=$W/stage41d/usr/local
else
    python3 "$L/stage_farm.py" verify "$E"
    stage=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["stage"])' "$E/stage-farm.json")
    export DARLING_INSTALL_PREFIX=$stage/usr/local
fi
export DPREFIX=$W/p/i386-${kind:0:1}-$(python3 -c 'import secrets; print(secrets.token_hex(3))')
[[ ${#DPREFIX} -le 83 && ! -e $DPREFIX ]]
for variable in ${!DYLD_@}; do unset "$variable"; done
unset DARLING_I386_STARTUP
mkdir -p "$E"
printf '%s\n' "$DPREFIX" > "$E/$kind-prefix.txt"
"$W/dashboard/qv-host-guard" snapshot "$E/$kind-host-folders.before"
cleanup() {
    local rc=$? shutdown_rc=0 guard_rc=0
    trap - EXIT
    DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher" || shutdown_rc=$?
    "$W/dashboard/qv-host-guard" verify "$E/$kind-host-folders.before" > "$E/$kind-host-folders.verify" 2>&1 || guard_rc=$?
    [[ $shutdown_rc == 0 ]] || rc=$shutdown_rc
    [[ $guard_rc == 0 ]] || rc=$guard_rc
    exit "$rc"
}
trap cleanup EXIT
# Let the launcher initialize the absent prefix before writing any guest file.
if ! "$launcher" shell /usr/bin/true > "$E/$kind-init-1.log" 2>&1; then
    DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
    "$launcher" shell /usr/bin/true > "$E/$kind-init-2.log" 2>&1
fi
DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
cp -a "$L/build/hello-i386" "$L/build/hello-x86_64" "$DPREFIX/Applications/"
printf 'case\tstatus\n' > "$E/$kind-results.tsv"
for round in 1 2; do
    for arch in x86_64 i386; do
        for option in 0 1; do
            name=$kind-$arch-option$option-round$round
            status=0
            "$launcher" shell /usr/bin/env "DARLING_I386_STARTUP=$option" "/Applications/hello-$arch" > "$E/$name.stdout" 2> "$E/$name.stderr" || status=$?
            if [[ $status != 0 ]] && grep -q 'Timed out waiting for shellspawn in container' "$E/$name.stderr"; then
                cp -a "$E/$name.stdout" "$E/$name.attempt1.stdout"
                cp -a "$E/$name.stderr" "$E/$name.attempt1.stderr"
                DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
                status=0
                "$launcher" shell /usr/bin/env "DARLING_I386_STARTUP=$option" "/Applications/hello-$arch" > "$E/$name.stdout" 2> "$E/$name.stderr" || status=$?
            fi
            printf '%s\t%s\n' "$name" "$status" >> "$E/$kind-results.tsv"
            DPREFIX=$DPREFIX "$W/dashboard/qv-shutdown" "$launcher"
        done
    done
done
