#!/usr/bin/env zsh

set -euo pipefail

oracle=${0:A:h}
repo=${oracle:h:h}
build=$("$repo/scripts/libfprint-build-dir.sh")
gallery=${GALLERY_TEMPLATE:-/tmp/chicago-boundary-final-20260713/native-template.bin}
out=${OUT_DIR:-/tmp/chicago-study-aggregate-parity}
vector=$out/study-aggregate.bin
test_binary=$build/tests/test-goodix-chicago-match

[[ -s $gallery ]] || {
  print -u2 -- "missing Chicago gallery: $gallery"
  print -u2 -- "set GALLERY_TEMPLATE or run run_live_20260713_enrollment_parity.sh"
  exit 2
}
mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 \
  "$oracle/chicago_study_aggregate_oracle.c" \
  -o "$oracle/chicago_study_aggregate_oracle.exe"
WINEDEBUG=-all wine "$oracle/chicago_study_aggregate_oracle.exe" \
  "$gallery" "$vector"
meson compile -C "$build" test-goodix-chicago-match
CHICAGO_MATCH_STUDY_AGGREGATE_VECTOR=$vector \
  "$test_binary" -p /gdix51c0/chicago-match/study-aggregate-q8-oracle
print -- "type-24 study aggregate parity: 128/128 exact (AlgoChicago+0x2a3e9)"
