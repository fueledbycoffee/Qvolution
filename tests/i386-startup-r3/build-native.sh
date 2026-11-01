#!/usr/bin/env bash
set -euo pipefail
W=/home/sean/Documents/Codex/Projects/Qvolution-work
L=$W/parallel/i386-guest-r3
S=$W/parallel/i386-guest/superproject
X=$S/src/external/xnu/darling/src/libsystem_kernel/emulation
export PATH=$W/dashboard/bin:$PATH
if [[ ${1-} != --slot ]]; then exec "$W/dashboard/qv-slot" build "$0" --slot; fi
mkdir -p "$L/build/include/darling"
[[ -e $L/build/include/darling/emulation ]] || ln -s "$X/include" "$L/build/include/darling/emulation"
/usr/bin/clang -fsyntax-only -I"$W/parallel/stage41d/build/src/external/darlingserver/include" "$S/tests/i386-startup-r3/rpc-layout.c"
for bits in 32 64; do
    /usr/bin/clang -m"$bits" -I"$L/build/include" -c "$X/src/xnu_syscall/machdep/impl/tls.c" -o "$L/build/tls-$bits.o"
    /usr/bin/clang -m"$bits" -I"$L/build/include" -Dsys_thread_set_tsd_base=second_thread_set_tsd_base -c "$X/src/xnu_syscall/machdep/impl/tls.c" -o "$L/build/tls-second-$bits.o"
    /usr/bin/clang -m"$bits" -Wall -Wextra -Werror "$S/tests/mldr-initial-tsd.c" "$L/native-syscall.c" "$L/build/tls-$bits.o" "$L/build/tls-second-$bits.o" -o "$L/build/initial-tsd-$bits"
done
