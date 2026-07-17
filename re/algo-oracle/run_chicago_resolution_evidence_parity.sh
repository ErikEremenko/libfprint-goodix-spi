#!/usr/bin/env zsh
# Exhaustively compare the canonical native packed-resolution map against the
# unmodified AlgoChicago+0x52bc0 decoder under Wine.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
driver=$root/gfspi.inf_amd64_9b379ce317eda6c3
out=${OUT_DIR:-/tmp/chicago-resolution-evidence-parity}
official=$out/official.txt
expected=$out/expected.txt
primary=(0 2 2 3)
secondary=(0 1 2 4 5 5 0 0)

mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 -g \
  "$oracle/chicago_resolution_evidence_oracle.c" \
  -o "$oracle/chicago_resolution_evidence_oracle.exe"
WINEPATH="$(WINEDEBUG=-all winepath -w "$driver")" WINEDEBUG=-all \
  wine "$oracle/chicago_resolution_evidence_oracle.exe" > "$official"

: > "$expected"
for high in {0..7}; do
  for low in {0..3}; do
    printf 'packed=0x%03x primary=%d secondary=%d\r\n' \
      $((high << 8 | low)) ${primary[low + 1]} ${secondary[high + 1]} \
      >> "$expected"
  done
done
diff -u "$expected" "$official"
print -- "packed-resolution parity: 32/32 exact (AlgoChicago+0x52bc0)"
