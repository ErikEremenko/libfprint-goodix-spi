#!/usr/bin/env zsh
# Compare the complete capacity-full templateStudy selector and mutation with
# the fresh Windows DLL, including closure, propagation, scores, replacement,
# packing, and CRC.

set -euo pipefail

oracle=${0:A:h}
root=${oracle:h:h}
dll_dir=$root/gfspi.inf_amd64_9b379ce317eda6c3
gallery=${GALLERY_TEMPLATE:?set GALLERY_TEMPLATE to a packed 50-subtemplate gallery}
probe=${PROBE_TEMPLATE:-$gallery}
selected=${SELECTED_INDEX:-0}
probe_index=${PROBE_INDEX:-$selected}
force_unresolved=${FORCE_UNRESOLVED_INDEX:-}
out=${OUT_DIR:-/tmp/chicago-capacity-selector-parity}
official=$out/official.bin
native=$out/native.bin
official_relations=$out/official-probe-relations.bin
native_relations=$out/native-probe-relations.bin
official_gallery_relations=$out/official-gallery-relations.bin
native_gallery_relations=$out/native-gallery-relations.bin
native_helper=$out/analyze-study-capacity
tap_offset=0x5d674
official_extra=()
native_extra=()

if [[ -n $force_unresolved ]]; then
  tap_offset=0x5caf0
  official_extra+=(
    "CHICAGO_CAPACITY_FORCE_UNRESOLVED_INDEX=$force_unresolved"
    "CHICAGO_CAPACITY_CHAIN_TAP_OFFSET=0x5d674"
  )
  native_extra+=("CHICAGO_CAPACITY_FORCE_UNRESOLVED_INDEX=$force_unresolved")
fi

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
env "${official_extra[@]}" \
CHICAGO_STUDY_APPEND_OUT="$(WINEDEBUG=-all winepath -w "$official")" \
CHICAGO_CAPACITY_RELATIONS_OUT="$(WINEDEBUG=-all winepath -w "$official_relations")" \
CHICAGO_CAPACITY_GALLERY_RELATIONS_OUT="$(WINEDEBUG=-all winepath -w "$official_gallery_relations")" \
CHICAGO_STUDY_SELECTED_INDEX=$selected \
CHICAGO_STUDY_PROBE_INDEX=$probe_index \
CHICAGO_STUDY_ONLY_PROBE=1 \
CHICAGO_SCORE_INDEX=0 \
CHICAGO_GALLERY_FIRST_INDEX=$selected \
CHICAGO_STUDY_ALLOW_REPLACE=1 \
CHICAGO_SCORE_TRACE=1 \
CHICAGO_TAP_OFFSET=$tap_offset \
WINEDEBUG=-all \
  wine "$oracle/chicago_identify_oracle.exe" "$gallery" "$probe" \
  > "$out/official.log"

env "${native_extra[@]}" \
CHICAGO_STUDY_PROBE_INDEX=$probe_index \
CHICAGO_GALLERY_FIRST_INDEX=$selected \
CHICAGO_CAPACITY_SELECT=1 \
CHICAGO_CAPACITY_RELATIONS_OUT=$native_relations \
CHICAGO_CAPACITY_GALLERY_RELATIONS_OUT=$native_gallery_relations \
  "$native_helper" "$gallery" "$probe" "$selected" "$native" \
  > "$out/native.log"

cmp "$official_relations" "$native_relations"
cmp "$official_gallery_relations" "$native_gallery_relations"
cmp "$official" "$native"

official_selector=$(rg -m1 '^capacity-selector ' "$out/official.log")
native_selector=$(rg -m1 '^native-capacity-selector ' "$out/native.log")
official_selector=${official_selector%$'\r'}
[[ ${official_selector#capacity-selector } == \
   ${native_selector#native-capacity-selector } ]]

digest=$(sha256sum "$official" | cut -d' ' -f1)
relation_digest=$(sha256sum "$official_relations" | cut -d' ' -f1)
print -- "Chicago capacity selector parity: exact ($(wc -c < "$official") bytes, sha256=$digest)"
print -- "Probe relation vector: exact ($(wc -c < "$official_relations") bytes, sha256=$relation_digest)"
rg -m1 '^native-capacity-replacement=' "$out/native.log"
