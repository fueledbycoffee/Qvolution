#!/bin/sh
set -eu
# Explicit source input: never silently link an unrelated installed/private library.
: "${BLOCKS_RUNTIME_SOURCE:?set to a vendored libdispatch src/BlocksRuntime directory}"
: "${AUDIO_NATIVE_BUILD:?set to an output directory outside the source checkout}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project=$(CDPATH= cd -- "$source_dir/../../.." && pwd)
blocks=$(CDPATH= cd -- "$BLOCKS_RUNTIME_SOURCE" && pwd)
mkdir -p "$AUDIO_NATIVE_BUILD"
out=$(CDPATH= cd -- "$AUDIO_NATIVE_BUILD" && pwd)
case "$out/" in "$project/"*) echo "AUDIO_NATIVE_BUILD must be outside the source checkout" >&2; exit 2;; esac
for name in runtime.c data.c Block.h Block_private.h; do test -f "$blocks/$name"; done
"${CC:-clang}" -c "$blocks/runtime.c" -I"$blocks" -o "$out/blocks-runtime.o"
"${CC:-clang}" -c "$blocks/data.c" -I"$blocks" -o "$out/blocks-data.o"
"${CXX:-clang++}" -std=c++17 -fblocks -Wno-nullability-completeness -Wno-multichar \
    -I"$source_dir/include" -I"$project/src/CoreAudio/include" \
    -I"$project/src/CoreAudio/CoreAudio" -I"$blocks" \
    "$source_dir/listener-control.cpp" "$source_dir/dispatch-adapter.cpp" \
    "$project/src/CoreAudio/CoreAudio/AudioHardwareImpl.cpp" \
    "$project/src/CoreAudio/CoreAudio/AudioHardwareStream.cpp" \
    "$project/src/CoreAudio/CoreAudio/AudioPropertyListeners.cpp" \
    "$out/blocks-runtime.o" "$out/blocks-data.o" -Wl,--wrap=_Block_copy \
    -pthread -o "$out/native-listener-control"
"$out/native-listener-control"
