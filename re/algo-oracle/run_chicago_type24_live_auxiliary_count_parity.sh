#!/usr/bin/env zsh
# Force AlgoChicago's normally rare type-24 +0x148 gate and compare the
# +0x528c0/+0x546b0 three-count output with the native affine-map path.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
build=$("$root/scripts/libfprint-build-dir.sh")
gallery=${GALLERY_TEMPLATE:-/tmp/chicago-boundary-final-20260713/native-template.bin}
out=${OUT_DIR:-/tmp/chicago-type24-live-auxiliary-count-parity}
vector=$out/vectors.bin

[[ -s $gallery ]] || {
  print -u2 -- "missing gallery template: $gallery"
  exit 1
}
mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_identify_oracle.c" \
  -o "$oracle/chicago_identify_oracle.exe"

CHICAGO_SCORE_TRACE=1 \
CHICAGO_TAP_OFFSET=0x510e0 \
CHICAGO_FORCE_AUX_PREFIX=3 \
CHICAGO_AUX_COUNT_VECTOR="$(WINEDEBUG=-all winepath -w "$vector")" \
WINEDEBUG=-all \
  wine "$oracle/chicago_identify_oracle.exe" "$gallery" "$gallery" \
  > "$out/official.log"

CHICAGO_LIVE_AUXILIARY_COUNT_VECTOR=$vector \
  "$build/tests/test-goodix-chicago-enrollment" \
  -p /gdix51c0/chicago-enrollment/live-auxiliary-count-oracle-type24

vectors=$(( $(wc -c < "$vector") / 52 ))
print -- "type-24 live auxiliary count parity: $vectors/$vectors exact (AlgoChicago+0x528c0/+0x546b0)"
