#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
# Keep output for review; the caller supplies a lane-local build directory.
out=${GAMEKIT_TEST_OUT:?set GAMEKIT_TEST_OUT to a lane-local build directory}
mkdir -p "$out"
source=${GAMEKIT_TEST_SOURCE:-$root/src/frameworks/GameKit/src/GameKit.m}
blocks_headers=${BLOCKS_RUNTIME_HEADERS:-/usr/include}
objc_include=$(gcc -print-file-name=include)
objc_lib=$(dirname "$(gcc -print-file-name=libobjc.so)")
blocks_flags=
if [ -n "${BLOCKS_RUNTIME_DIR:-}" ]; then
    blocks_flags="-L$BLOCKS_RUNTIME_DIR -Wl,-rpath,$BLOCKS_RUNTIME_DIR"
fi
cat > "$out/build.ninja" <<NINJA
rule test
  command = clang -fblocks -fconstant-string-class=NSConstantString -I"$root/tests/gamekit-native" -I"$root/src/frameworks/GameKit/include" -I"$objc_include" -I"$blocks_headers" "$source" "$root/tests/gamekit-native/authenticate.m" -L"$objc_lib" -lobjc $blocks_flags -lBlocksRuntime -pthread -Wl,--wrap=objc_msg_lookup -o "$out/authenticate"
build $out/authenticate: test $source $root/src/frameworks/GameKit/include/GameKit/GameKit.h $root/tests/gamekit-native/authenticate.m $root/tests/gamekit-native/Foundation/Foundation.h $root/tests/gamekit-native/AppKit/AppKit.h $root/tests/gamekit-native/dispatch/dispatch.h
NINJA
ninja -C "$out" -n
ninja -C "$out" -j6
"$out/authenticate"
