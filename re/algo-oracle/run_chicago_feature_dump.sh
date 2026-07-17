#!/usr/bin/env zsh
# Extract production Chicago feature arrays from the exact enhanced vectors.

set -euo pipefail

oracle=${0:A:h}
frames=$oracle/newraws
base=${BASE_RAW:-$frames/gdix51c0_nofinger_46943865219.raw}
enhanced_dir=${FEATURE_ENHANCED_DIR:-/tmp/chicago-candidate-parity}
out=${OUT_DIR:-/tmp/chicago-feature-vectors}

mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_feature_dump.c" \
  -o "$oracle/chicago_feature_dump.exe"

for frame in $frames/gdix51c0_frame_*.raw; do
  stem=${frame:t:r}
  enhanced="$enhanced_dir/$stem.candidate"
  metadata="$enhanced_dir/$stem.base-quality.meta"

  if [[ ! -f $enhanced || ! -f $metadata ]]; then
    print -u2 -- "missing enhanced parity vector for $stem; run run_chicago_candidate_parity.sh first"
    exit 1
  fi

  WINEDEBUG=-all wine "$oracle/chicago_feature_dump.exe" \
    "$base" "$frame" "$enhanced" "$metadata" "$out/$stem.features"
done
