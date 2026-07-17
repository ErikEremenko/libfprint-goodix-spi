#!/usr/bin/env zsh
# Compare the complete under-capacity templateStudy packed output against
# AlgoChicago+0x5dd60/+0x5ea80, including metadata, graph state, and CRC.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
dll_dir=$root/gfspi.inf_amd64_9b379ce317eda6c3
gallery=${GALLERY_TEMPLATE:-/tmp/chicago-boundary-final-20260713/native-template.bin}
probe=${PROBE_TEMPLATE:-$gallery}
selected=${SELECTED_INDEX:-0}
out=${OUT_DIR:-/tmp/chicago-study-append-parity}
official=$out/official.bin
native=$out/native.bin
native_helper=$out/analyze-study-append

[[ -s $gallery ]] || {
  print -u2 -- "missing gallery template: $gallery"
  exit 1
}
[[ -s $probe ]] || {
  print -u2 -- "missing probe template: $probe"
  exit 1
}
mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 -g \
  "$oracle/chicago_identify_oracle.c" \
  -o "$oracle/chicago_identify_oracle.exe"
cc -O2 -g "$root/re/analyze_study_append.c" -o "$native_helper" \
  $(pkg-config --cflags --libs gio-2.0 glib-2.0) -lm

export WINEPATH="$(WINEDEBUG=-all winepath -w "$dll_dir")"
CHICAGO_STUDY_APPEND_OUT="$(WINEDEBUG=-all winepath -w "$official")" \
CHICAGO_STUDY_SELECTED_INDEX=$selected \
WINEDEBUG=-all \
  wine "$oracle/chicago_identify_oracle.exe" "$gallery" "$probe" \
  > "$out/official.log"
"$native_helper" "$gallery" "$probe" "$selected" "$native" \
  > "$out/native.log"

cmp "$official" "$native"
digest=$(sha256sum "$official" | cut -d' ' -f1)
print -- "Chicago study append parity: exact ($(wc -c < "$official") bytes, sha256=$digest)"
