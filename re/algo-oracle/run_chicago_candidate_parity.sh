#!/usr/bin/env zsh
# Compare native raw preparation through enhanced output with AlgoChicago.

set -euo pipefail

oracle=${0:A:h}
repo=${oracle:h:h}
build=$("$repo/scripts/libfprint-build-dir.sh")
frames=$oracle/newraws
base=${BASE_RAW:-$frames/gdix51c0_nofinger_46943865219.raw}
calibration=${CALIBRATION_FILE:-$repo/re/private/goodix_calib.dat}
out=${OUT_DIR:-/tmp/chicago-candidate-parity}
native_test=$build/tests/test-goodix-chicago-preprocess

[[ -f $calibration ]] || {
  print -u2 -- "CALIBRATION_FILE must point to a private per-unit goodix_calib.dat"
  exit 1
}

mkdir -p "$out"
meson compile -C "$build" test-goodix-chicago-preprocess
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_preprocess_dump.c" \
  -o "$oracle/chicago_preprocess_dump.exe"

for frame in $frames/gdix51c0_frame_*.raw; do
  stem=${frame:t:r}

  for spec in \
    48f62:rcx:0:10240:current \
    48f62:rdx:0:10240:base \
    48f67:rbx:0x13244:10240:source \
    48f9e:r9:0x10:5120:mask \
    48fa3:rbx:0x1cb64:5120:candidate \
    1032a:rbx:0:4:base-quality; do
    rva=${spec%%:*}
    rest=${spec#*:}
    reg=${rest%%:*}
    rest=${rest#*:}
    offset=${rest%%:*}
    rest=${rest#*:}
    bytes=${rest%%:*}
    name=${rest#*:}

    CHICAGO_TAP_PHASE=init \
    CHICAGO_TAP_RVA=$rva \
    CHICAGO_TAP_REG=$reg \
    CHICAGO_TAP_ADD_OFFSET=$offset \
    CHICAGO_TAP_BYTES=$bytes \
    CHICAGO_TAP_OUT="$out/$stem.$name" \
    WINEDEBUG=-all \
      wine "$oracle/chicago_preprocess_dump.exe" \
        "$base" "$frame" \
        "$out/$stem.$name.enh" "$out/$stem.$name.meta" >/dev/null
  done

  CHICAGO_TAP_RVA=102ed \
  CHICAGO_TAP_REG=r14 \
  CHICAGO_TAP_RSP_OFFSET=0x70 \
  CHICAGO_TAP_ADD_OFFSET=0x18 \
  CHICAGO_TAP_POST_DEREFS=1 \
  CHICAGO_TAP_BYTES=5120 \
  CHICAGO_TAP_OUT="$out/$stem.quality-mask" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" \
      "$out/$stem.quality-mask.enh" \
      "$out/$stem.quality-mask.meta" >/dev/null

  CHICAGO_SOURCE_CURRENT="$out/$stem.current" \
  CHICAGO_SOURCE_BASE="$out/$stem.base" \
  CHICAGO_SOURCE_EXPECTED="$out/$stem.source" \
  CHICAGO_CALIBRATION_FILE="$calibration" \
  CHICAGO_SOURCE_MASK="$out/$stem.mask" \
  CHICAGO_ENHANCED_EXPECTED="$out/$stem.candidate.enh" \
  CHICAGO_RAW_CURRENT="$frame" \
  CHICAGO_RAW_BASE="$base" \
    "$native_test" \
      -p /gdix51c0/chicago-preprocess/matches-oracle-source-plane >/dev/null

  CHICAGO_STAGE_SOURCE="$out/$stem.source" \
  CHICAGO_STAGE_MASK="$out/$stem.mask" \
  CHICAGO_STAGE_CANDIDATE="$out/$stem.candidate" \
    "$native_test" \
      -p /gdix51c0/chicago-preprocess/matches-oracle-candidate >/dev/null
  CHICAGO_BASE_QUALITY="$out/$stem.base-quality" \
  CHICAGO_METRICS_METADATA="$out/$stem.base-quality.meta" \
    "$native_test" \
      -p /gdix51c0/chicago-preprocess/matches-oracle-metrics >/dev/null
  CHICAGO_QUALITY_ENHANCED="$out/$stem.candidate" \
  CHICAGO_QUALITY_MASK="$out/$stem.quality-mask" \
  CHICAGO_BASE_QUALITY="$out/$stem.base-quality" \
    "$native_test" \
      -p /gdix51c0/chicago-preprocess/matches-oracle-base-quality >/dev/null
  CHICAGO_COVERAGE_ENHANCED="$out/$stem.candidate" \
  CHICAGO_COVERAGE_METADATA="$out/$stem.base-quality.meta" \
    "$native_test" \
      -p /gdix51c0/chicago-preprocess/matches-oracle-coverage >/dev/null
  cmp "$out/$stem.candidate" "$out/$stem.candidate.enh"
  print -- "$stem raw=exact source=exact mask=exact enhanced=exact base-quality=exact coverage=exact metrics-wrapper=exact"
done
