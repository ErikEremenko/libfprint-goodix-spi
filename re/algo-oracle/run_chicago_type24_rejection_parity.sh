#!/usr/bin/env zsh
# Generate a deterministic decision corpus with the untouched production DLL
# and replay it through the native +0x2ce50 type-24 translation.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
driver=$root/gfspi.inf_amd64_9b379ce317eda6c3
build=$("$root/scripts/libfprint-build-dir.sh")
out=${OUT_DIR:-/tmp/chicago-type24-rejection-parity}
vector=$out/rejection-corpus.bin

mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra \
  "$oracle/chicago_type24_rejection_oracle.c" \
  -o "$oracle/chicago_type24_rejection_oracle.exe"
WINEPATH="$(WINEDEBUG=-all winepath -w "$driver")" \
CHICAGO_REJECTION_VECTOR="$(WINEDEBUG=-all winepath -w "$vector")" \
WINEDEBUG=-all wine "$oracle/chicago_type24_rejection_oracle.exe"

"$root/scripts/libfprint/prepare-tree.sh" "${build:h}"
meson compile -C "$build" test-goodix-chicago-match
CHICAGO_MATCH_REJECTION_VECTOR="$vector" \
  "$build/tests/goodix-spi/test-goodix-chicago-match" \
  -p /gdix51c0/chicago-match/late-rejection-type24-oracle
print -- "type-24 late-rejection parity: 512/512 exact (AlgoChicago+0x2ce50)"
