#!/usr/bin/env zsh
# Reproduce the production EngineAdapter cross-capture match under Wine.
#
# This is an RE oracle: it runs the original EngineAdapter.dll and
# AlgoChicago.dll. engine_probe.exe replaces only the WBF storage callback
# with RAM-backed storage, and uses the captured newraws for sensor replies.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
frames=$oracle/newraws
base=$frames/gdix51c0_nofinger_46943865219.raw
driver=$root/gfspi.inf_amd64_9b379ce317eda6c3
otp_fixture=${ENGINE_OTP_FILE:-$root/re/private/gdix51c0-otp.bin}
order=(
  $frames/gdix51c0_frame_46941642439.raw
  $frames/gdix51c0_frame_46949039139.raw
  $frames/gdix51c0_frame_46940553896.raw
  $frames/gdix51c0_frame_46949880564.raw
  $frames/gdix51c0_frame_46945001058.raw
  $frames/gdix51c0_frame_46938733390.raw
  $frames/gdix51c0_frame_46943411976.raw
  $frames/gdix51c0_frame_46946678991.raw
  $frames/gdix51c0_frame_46947478034.raw
  $frames/gdix51c0_frame_46945864087.raw
  $frames/gdix51c0_frame_46942554085.raw
  $frames/gdix51c0_frame_46950643211.raw
  $frames/gdix51c0_frame_46944174638.raw
  $frames/gdix51c0_frame_46939559130.raw
  $frames/gdix51c0_frame_46951448989.raw
  $frames/gdix51c0_frame_46948249312.raw
  $frames/gdix51c0_frame_46937405529.raw
)

[[ -f $otp_fixture && $(wc -c < "$otp_fixture") == 64 ]] || {
  print -u2 -- "ENGINE_OTP_FILE must point to a private 64-byte OTP fixture"
  exit 1
}

export WINEPATH="$(WINEDEBUG=-all winepath -w "$driver")"
export ENGINE_OTP_FILE="$(WINEDEBUG=-all winepath -w "$otp_fixture")"
export ATTACH=1 ENGINE_MATCH=1 ENGINE_GALLERY_COUNT=17
export ENGINE_USE_ALL=1 ENGINE_CONTINUE_BAD=1
export SENSOR_BASE_RAW="$base" INJECT_BASE=1 TRACE_INIT=1
export DUMP_STORED="${DUMP_STORED:-/tmp/chicago-engine-template.bin}"
export WINEDEBUG=-all

# Frame 46938733390 is deliberately retained in the input sequence but is
# rejected by UpdateEnrollment (detail 4), so it is absent from the accepted
# 12-sample template and is a cross-capture identify probe here.
exec wine "$oracle/engine_probe.exe" "${order[@]}" \
  "$frames/gdix51c0_frame_46938733390.raw"
