#!/usr/bin/env zsh

set -euo pipefail

oracle=${0:A:h}
repo=${oracle:h:h}
build=$("$repo/scripts/libfprint-build-dir.sh")
out=${OUT_DIR:-/tmp/chicago-type24-prefilter-parity}
vector=$out/type24-prefilter.bin
test_binary=$build/tests/test-goodix-chicago-match

mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 \
  "$oracle/chicago_type24_prefilter_oracle.c" \
  -o "$oracle/chicago_type24_prefilter_oracle.exe"
WINEDEBUG=-all wine "$oracle/chicago_type24_prefilter_oracle.exe" "$vector"
meson compile -C "$build" test-goodix-chicago-match
CHICAGO_MATCH_PREFILTER_VECTOR=$vector \
  "$test_binary" -p /gdix51c0/chicago-match/candidate-prefilter-type24-oracle
print -- "type-24 candidate prefilter parity: 512/512 exact (AlgoChicago+0x245c0)"
