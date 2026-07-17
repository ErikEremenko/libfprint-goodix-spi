#!/usr/bin/env zsh
# Compare AlgoChicago+0x15e70 input, magnitude, and orientation with native C.

set -euo pipefail

oracle=${0:A:h}
repo=${oracle:h:h}
build=$("$repo/scripts/libfprint-build-dir.sh")
frames=$oracle/newraws
base=${BASE_RAW:-$frames/gdix51c0_nofinger_46943865219.raw}
out=${OUT_DIR:-/tmp/chicago-feature-gradient-parity}
native_test=$build/tests/test-goodix-chicago-feature

mkdir -p "$out"
meson compile -C "$build" test-goodix-chicago-feature
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_preprocess_dump.c" \
  -o "$oracle/chicago_preprocess_dump.exe"

for frame in $frames/gdix51c0_frame_*.raw; do
  stem=${frame:t:r}
  vector_dir="$out/$stem"
  mkdir -p "$vector_dir"

  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=114e0 \
  CHICAGO_TAP_REG=rcx \
  CHICAGO_TAP_ADD_OFFSET=0x18 \
  CHICAGO_TAP_POST_DEREFS=1 \
  CHICAGO_TAP_BYTES=5120 \
  CHICAGO_TAP_OUT="$vector_dir/feature-image.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/feature-image.enhanced" \
      "$vector_dir/feature-image.metadata" >/dev/null

  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=55a27 \
  CHICAGO_TAP_REG=rsp \
  CHICAGO_TAP_ADD_OFFSET=0x30 \
  CHICAGO_TAP_BYTES=5120 \
  CHICAGO_TAP_OUT="$vector_dir/orientation-map.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/orientation-map.enhanced" \
      "$vector_dir/orientation-map.metadata" >/dev/null
  cmp "$vector_dir/feature-image.enhanced" \
      "$vector_dir/orientation-map.enhanced"

  for spec in input:-80:20480 magnitude:-96:20480 orientation:-88:10240; do
    name=${spec%%:*}
    rest=${spec#*:}
    offset=${rest%%:*}
    bytes=${rest##*:}

    CHICAGO_EXTRACT_FEATURES=1 \
    CHICAGO_TAP_RVA=117bb \
    CHICAGO_TAP_REG=rbp \
    CHICAGO_TAP_PRE_ADD_OFFSET=$offset \
    CHICAGO_TAP_DEREFS=1 \
    CHICAGO_TAP_ADD_OFFSET=0x18 \
    CHICAGO_TAP_POST_DEREFS=1 \
    CHICAGO_TAP_BYTES=$bytes \
    CHICAGO_TAP_OUT="$vector_dir/$name.bin" \
    WINEDEBUG=-all \
      wine "$oracle/chicago_preprocess_dump.exe" \
        "$base" "$frame" "$vector_dir/$name.enhanced" \
        "$vector_dir/$name.metadata" >/dev/null
  done

  cmp "$vector_dir/input.enhanced" "$vector_dir/magnitude.enhanced"
  cmp "$vector_dir/input.enhanced" "$vector_dir/orientation.enhanced"
  CHICAGO_FEATURE_GRADIENT_IMAGE="$vector_dir/feature-image.bin" \
  CHICAGO_FEATURE_ENHANCED="$vector_dir/feature-image.enhanced" \
  CHICAGO_FEATURE_ORIENTATION_MAP="$vector_dir/orientation-map.bin" \
  CHICAGO_FEATURE_GRADIENT_INPUT="$vector_dir/input.bin" \
  CHICAGO_FEATURE_MAGNITUDE="$vector_dir/magnitude.bin" \
  CHICAGO_FEATURE_ORIENTATION="$vector_dir/orientation.bin" \
    "$native_test" \
      -p /gdix51c0/chicago-feature/matches-oracle-gradients >/dev/null
  print -- "$stem prepared=exact gradient-input=exact magnitude=exact orientation=exact"
done
