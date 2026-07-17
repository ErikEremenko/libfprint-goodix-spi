#!/usr/bin/env zsh
# Dump production Chicago enhanced-image output for every fresh raw capture.

set -euo pipefail

oracle=${0:A:h}
frames=$oracle/newraws
base=${BASE_RAW:-$frames/gdix51c0_nofinger_46943865219.raw}
out=${OUT_DIR:-/tmp/chicago-preprocess}

mkdir -p "$out"

x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_preprocess_dump.c" \
  -o "$oracle/chicago_preprocess_dump.exe"

for frame in $frames/gdix51c0_frame_*.raw; do
  stem=${frame:t:r}
  WINEDEBUG=-all wine "$oracle/chicago_preprocess_dump.exe" \
    "$base" "$frame" "$out/$stem.enh" "$out/$stem.meta"
done
