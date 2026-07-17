#!/usr/bin/env zsh
# Replay the fresh 2026-07-13 hardware enrollment through native Chicago and
# the unmodified production EngineAdapter/AlgoChicago DLLs.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
dataset=$oracle/newraws/live-2322
manifest=$dataset/enrollment-20260713-1032.tsv
driver=$root/gfspi.inf_amd64_9b379ce317eda6c3
otp_fixture=${ENGINE_OTP_FILE:-$root/re/private/gdix51c0-otp.bin}
out=${OUT_DIR:-/tmp/chicago-live-20260713-parity}
native=$out/analyze-enrollment-sequence
frames=()

mkdir -p "$out"
while IFS=$' \t' read -r filename _; do
  [[ -z $filename || $filename == \#* ]] && continue
  frames+=("$dataset/$filename")
done < "$manifest"

if (( ${#frames} != 23 )); then
  print -u2 -- "expected 23 enrollment captures, found ${#frames}"
  exit 1
fi
for frame in "${frames[@]}"; do
  [[ -s $frame ]] || { print -u2 -- "missing capture: $frame"; exit 1; }
done
[[ -f $otp_fixture && $(wc -c < "$otp_fixture") == 64 ]] || {
  print -u2 -- "ENGINE_OTP_FILE must point to a private 64-byte OTP fixture"
  exit 1
}

cc -O2 -g "$root/re/analyze_enrollment_sequence.c" \
  -o "$native" \
  $(pkg-config --cflags --libs gio-2.0 glib-2.0) -lm
"$native" \
  "$dataset/gdix51c0-calibration.bin" \
  "$dataset/gdix51c0-image-base.raw" \
  "$out/native-template.bin" \
  3 \
  "${frames[@]}" | tee "$out/native-trace.log"

x86_64-w64-mingw32-gcc -O2 -g "$oracle/engine_probe.c" \
  -o "$oracle/engine_probe.exe"
export WINEPATH="$(WINEDEBUG=-all winepath -w "$driver")"
export ENGINE_OTP_FILE="$(WINEDEBUG=-all winepath -w "$otp_fixture")"
export CHICAGO_CAL="$(WINEDEBUG=-all winepath -w \
  "$dataset/gdix51c0-calibration.bin")"
export ATTACH=1 ENGINE_MATCH=1 ENGINE_GALLERY_COUNT=${#frames}
export ENGINE_USE_ALL=1 ENGINE_CONTINUE_BAD=1
export ENGINE_WARMUP_COUNT=3
export SENSOR_BASE_RAW="$dataset/gdix51c0-image-base.raw" INJECT_BASE=1
export TRACE_INIT=1
export DUMP_STORED="$out/official-system-template.bin"
export WINEDEBUG=-all
if [[ ${TRACE_ENROLL_POLICY:-0} == 1 ]]; then
  export TRACE_ENROLL_POLICY=1
fi

wine "$oracle/engine_probe.exe" "${frames[@]}" \
  "$dataset/gdix51c0_frame_40156486750.raw" | \
  tee "$out/official-trace.log"

sha256sum "$out/native-template.bin" "$out/official-system-template.bin" | \
  tee "$out/template-sha256.txt"
