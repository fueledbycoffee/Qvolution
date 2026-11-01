#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
clang -I"$root/tests/geokit-native" -Wno-objc-property-implementation \
  "$root/src/private-frameworks/GeoKit/src/GeoKit.m" \
  "$root/tests/geokit-native/system-city.m" \
  -L"$(gcc -print-file-name=libobjc.so | xargs dirname)" -lobjc -o "$out/test"
"$out/test"
