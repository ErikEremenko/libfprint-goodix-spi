#!/usr/bin/env zsh
# Compare the native live subtemplate +0x148 record with the unmodified
# EngineAdapter/AlgoChicago producer over the 20-frame hardware enrollment.

set -euo pipefail

oracle=${0:A:h}
out=${OUT_DIR:-/tmp/chicago-live-auxiliary-parity}
trace=$out/trace.log
native=$out/native.txt
official=$out/official.txt

mkdir -p "$out"
TRACE_AUXILIARY=1 WINEDEBUG=-all \
  "$oracle/run_live_20260713_enrollment_parity.sh" > "$trace"

awk '/live-auxiliary-native/ {
       sub(/^.*values=/, ""); print
     }' "$trace" > "$native"

# EngineAdapter reconstructs one deferred capture twice.  Deduplicate by its
# raw-frame path and stop after the 20 enrollment inputs, before the final
# identify probe.
awk '/Chicago live auxiliary\[/ {
       match($0, /frame=([^ ]+)/, frame)
       if (!(frame[1] in seen) && count < 20) {
         seen[frame[1]] = 1
         match($0, /values=([^ ]+)/, values)
         print values[1]
         count++
       }
     }
     END { if (count != 20) exit 2 }' "$trace" > "$official"

[[ $(wc -l < "$native") == 20 ]] || {
  print -u2 -- "expected 20 native auxiliary records"
  exit 1
}
diff -u "$official" "$native"
print -- "live auxiliary parity: 20/20 exact (subtemplate +0x148)"
