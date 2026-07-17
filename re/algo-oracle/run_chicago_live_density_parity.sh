#!/usr/bin/env zsh
# Compare AlgoChicago's transient live-probe +0x158/+0x15c/+0x160 outputs
# with the native +0x12400 reproduction over captured enhanced vectors.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
dataset=${DATASET:-$oracle/newraws/live-2322}
vectors=${VECTOR_DIR:-/tmp/live-2322-official}
out=${OUT_DIR:-/tmp/chicago-live-density-parity}
official=$out/official.txt
native=$out/native.txt
analyzer=$out/analyze-feature-file

mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_feature_dump.c" \
  -o "$oracle/chicago_feature_dump.exe"
cc -O2 -g "$root/re/analyze_feature_file.c" -o "$analyzer" \
  $(pkg-config --cflags --libs glib-2.0) -lm

export CHICAGO_CAL="$(WINEDEBUG=-all winepath -w \
  "$dataset/gdix51c0-calibration.bin")"
export CHICAGO_ACCEPT_ENHANCED_VECTOR=1
export CHICAGO_FEATURE_GEOMETRY_80X64=1
export CHICAGO_STUDY_TAP=1
export WINEDEBUG=-all
: > "$official"
: > "$native"

count=0
for enhanced in "$vectors"/*.enh; do
  stem=${enhanced:t:r}
  frame="$dataset/gdix51c0_frame_$stem.raw"
  metadata="$vectors/$stem.meta"

  [[ -s $frame && -s $metadata ]] || continue
  wine "$oracle/chicago_feature_dump.exe" \
    "$dataset/gdix51c0-image-base.raw" "$frame" "$enhanced" "$metadata" \
    "$out/$stem.features" | tr -d '\r' | \
    awk -v s="$stem" '$1 == "live-density" {print s, $3, $4, $5}' >> "$official"
  "$analyzer" "$enhanced" | sed -E \
    "s/^.*density=([^ ]+) class=([^ ]+) inactive=([^ ]+).*$/<STEM> percentages=\\1 class=\\2 inactive=\\3/" | \
    sed "s/<STEM>/$stem/" >> "$native"
  (( ++count ))
done

if (( count == 0 )); then
  print -u2 -- "no enhanced/meta/raw vector triples found"
  exit 1
fi
diff -u "$official" "$native"
print -- "live-density parity: $count/$count exact (+0x158/+0x15c/+0x160)"
