#!/usr/bin/env zsh
# Verify the capacity-full +0x5d550 mutation after the official pre-selector
# helpers have produced the relation vector and replacement index.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
dll_dir=$root/gfspi.inf_amd64_9b379ce317eda6c3
gallery=${GALLERY_TEMPLATE:?set GALLERY_TEMPLATE to a packed 50-subtemplate gallery}
probe=${PROBE_TEMPLATE:-$gallery}
selected=${SELECTED_INDEX:-0}
probe_index=${PROBE_INDEX:-$selected}
out=${OUT_DIR:-/tmp/chicago-study-replacement-mutation-parity}
official=$out/official.bin
native=$out/native.bin
relations=$out/official-relations.bin
native_helper=$out/analyze-study-mutate

mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 -g \
  "$oracle/chicago_identify_oracle.c" \
  -o "$oracle/chicago_identify_oracle.exe"
cc -O2 -g "$root/re/analyze_study_append.c" -o "$native_helper" \
  $(pkg-config --cflags --libs gio-2.0 glib-2.0) -lm

export WINEPATH="$(WINEDEBUG=-all winepath -w "$dll_dir")"
CHICAGO_STUDY_APPEND_OUT="$(WINEDEBUG=-all winepath -w "$official")" \
CHICAGO_CAPACITY_RELATIONS_OUT="$(WINEDEBUG=-all winepath -w "$relations")" \
CHICAGO_STUDY_SELECTED_INDEX=$selected \
CHICAGO_STUDY_PROBE_INDEX=$probe_index \
CHICAGO_STUDY_ONLY_PROBE=1 \
CHICAGO_SCORE_INDEX=0 \
CHICAGO_GALLERY_FIRST_INDEX=$selected \
CHICAGO_STUDY_ALLOW_REPLACE=1 \
CHICAGO_SCORE_TRACE=1 \
CHICAGO_TAP_OFFSET=0x5d674 \
WINEDEBUG=-all \
  wine "$oracle/chicago_identify_oracle.exe" "$gallery" "$probe" \
  > "$out/official.log"

CHICAGO_STUDY_PROBE_INDEX=$probe_index \
CHICAGO_GALLERY_FIRST_INDEX=$selected \
CHICAGO_CAPACITY_RELATIONS_IN=$relations \
  "$native_helper" "$gallery" "$probe" "$selected" "$native" \
  > "$out/native.log"

cmp "$official" "$native"
digest=$(sha256sum "$official" | cut -d' ' -f1)
print -- "Chicago capacity mutation parity: exact ($(wc -c < "$official") bytes, sha256=$digest)"
