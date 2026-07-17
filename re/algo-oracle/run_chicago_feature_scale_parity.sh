#!/usr/bin/env zsh
# Compare the native feature source and nine Gaussian scales with AlgoChicago.

set -euo pipefail

oracle=${0:A:h}
repo=${oracle:h:h}
build=$("$repo/scripts/libfprint-build-dir.sh")
frames=$oracle/newraws
base=${BASE_RAW:-$frames/gdix51c0_nofinger_46943865219.raw}
out=${OUT_DIR:-/tmp/chicago-feature-scale-parity}
native_test=$build/tests/test-goodix-chicago-feature

mkdir -p "$out"
meson compile -C "$build" test-goodix-chicago-feature
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_preprocess_dump.c" \
  -o "$oracle/chicago_preprocess_dump.exe"
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_extrema_oracle.c" \
  -o "$oracle/chicago_extrema_oracle.exe"
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_refinement_oracle.c" \
  -o "$oracle/chicago_refinement_oracle.exe"
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_materialization_oracle.c" \
  -o "$oracle/chicago_materialization_oracle.exe"
x86_64-w64-mingw32-gcc -O2 "$oracle/chicago_descriptor_oracle.c" \
  -o "$oracle/chicago_descriptor_oracle.exe"

for frame in $frames/gdix51c0_frame_*.raw; do
  stem=${frame:t:r}
  vector_dir="$out/$stem"
  mkdir -p "$vector_dir"

  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=1457a \
  CHICAGO_TAP_REG=rdx \
  CHICAGO_TAP_ADD_OFFSET=0x18 \
  CHICAGO_TAP_POST_DEREFS=1 \
  CHICAGO_TAP_BYTES=5120 \
  CHICAGO_TAP_OUT="$vector_dir/source.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/enhanced.bin" \
      "$vector_dir/metadata.bin" >/dev/null

  for scale in {0..8}; do
    CHICAGO_EXTRACT_FEATURES=1 \
    CHICAGO_TAP_RVA=11651 \
    CHICAGO_TAP_REG=r13 \
    CHICAGO_TAP_PRE_ADD_OFFSET=$((scale * 8)) \
    CHICAGO_TAP_DEREFS=1 \
    CHICAGO_TAP_ADD_OFFSET=0x18 \
    CHICAGO_TAP_POST_DEREFS=1 \
    CHICAGO_TAP_BYTES=10240 \
    CHICAGO_TAP_OUT="$vector_dir/scale-$scale.bin" \
    WINEDEBUG=-all \
      wine "$oracle/chicago_preprocess_dump.exe" \
        "$base" "$frame" "$vector_dir/scale-$scale.enhanced" \
        "$vector_dir/scale-$scale.metadata" >/dev/null
    cmp "$vector_dir/enhanced.bin" "$vector_dir/scale-$scale.enhanced"
  done

  for spec in magnitude:-96:20480 orientation:-88:10240; do
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
    cmp "$vector_dir/enhanced.bin" "$vector_dir/$name.enhanced"
  done

  WINEDEBUG=-all wine "$oracle/chicago_extrema_oracle.exe" \
    "$vector_dir"/scale-{0..8}.bin "$vector_dir/extrema.bin" >/dev/null
  WINEDEBUG=-all wine "$oracle/chicago_refinement_oracle.exe" \
    "$vector_dir"/scale-{0..8}.bin "$vector_dir/refinement.bin" >/dev/null
  WINEDEBUG=-all wine "$oracle/chicago_materialization_oracle.exe" \
    "$vector_dir"/scale-{0..8}.bin "$vector_dir/source.bin" \
    "$vector_dir/magnitude.bin" "$vector_dir/orientation.bin" \
    "$vector_dir/materialization.bin" >/dev/null
  WINEDEBUG=-all wine "$oracle/chicago_descriptor_oracle.exe" \
    "$vector_dir/materialization.bin" "$vector_dir/magnitude.bin" \
    "$vector_dir/orientation.bin" "$vector_dir/descriptor.bin" >/dev/null
  descriptor_count=$(od -An -tu4 -N4 "$vector_dir/descriptor.bin")
  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=11ac9 \
  CHICAGO_TAP_REG=r8 \
  CHICAGO_TAP_BYTES=$((descriptor_count * 0x3c)) \
  CHICAGO_TAP_OUT="$vector_dir/finalized.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/finalized.enhanced" \
      "$vector_dir/finalized.metadata" >/dev/null
  cmp "$vector_dir/enhanced.bin" "$vector_dir/finalized.enhanced"

  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=146a0 \
  CHICAGO_TAP_REG=rbp \
  CHICAGO_TAP_PRE_ADD_OFFSET=-0x18 \
  CHICAGO_TAP_DEREFS=1 \
  CHICAGO_TAP_ADD_OFFSET=0x18 \
  CHICAGO_TAP_POST_DEREFS=1 \
  CHICAGO_TAP_BYTES=5120 \
  CHICAGO_TAP_OUT="$vector_dir/feature-mask.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/feature-mask.enhanced" \
      "$vector_dir/feature-mask.metadata" >/dev/null
  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=146a0 \
  CHICAGO_TAP_REG=rdi \
  CHICAGO_TAP_DEREFS=1 \
  CHICAGO_TAP_ADD_OFFSET=0xf8 \
  CHICAGO_TAP_POST_DEREFS=1 \
  CHICAGO_TAP_BYTES=$((descriptor_count * 0x3c)) \
  CHICAGO_TAP_OUT="$vector_dir/post-mask.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/post-mask.enhanced" \
      "$vector_dir/post-mask.metadata" >/dev/null
  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=148d3 \
  CHICAGO_TAP_REG=rdi \
  CHICAGO_TAP_DEREFS=1 \
  CHICAGO_TAP_ADD_OFFSET=0xf8 \
  CHICAGO_TAP_POST_DEREFS=1 \
  CHICAGO_TAP_BYTES=$((descriptor_count * 0x3c)) \
  CHICAGO_TAP_OUT="$vector_dir/post-quality.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/post-quality.enhanced" \
      "$vector_dir/post-quality.metadata" >/dev/null
  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=148d3 \
  CHICAGO_TAP_REG=rdi \
  CHICAGO_TAP_DEREFS=1 \
  CHICAGO_TAP_ADD_OFFSET=0x164 \
  CHICAGO_TAP_BYTES=$descriptor_count \
  CHICAGO_TAP_OUT="$vector_dir/feature-annotations.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/feature-annotations.enhanced" \
      "$vector_dir/feature-annotations.metadata" >/dev/null
  CHICAGO_EXTRACT_FEATURES=1 \
  CHICAGO_TAP_RVA=1464d \
  CHICAGO_TAP_REG=rdi \
  CHICAGO_TAP_DEREFS=1 \
  CHICAGO_TAP_ADD_OFFSET=0x164 \
  CHICAGO_TAP_BYTES=$descriptor_count \
  CHICAGO_TAP_OUT="$vector_dir/pre-filter-annotations.bin" \
  WINEDEBUG=-all \
    wine "$oracle/chicago_preprocess_dump.exe" \
      "$base" "$frame" "$vector_dir/pre-filter-annotations.enhanced" \
      "$vector_dir/pre-filter-annotations.metadata" >/dev/null
  cmp "$vector_dir/enhanced.bin" "$vector_dir/feature-mask.enhanced"
  cmp "$vector_dir/enhanced.bin" "$vector_dir/post-mask.enhanced"
  cmp "$vector_dir/enhanced.bin" "$vector_dir/post-quality.enhanced"
  cmp "$vector_dir/enhanced.bin" "$vector_dir/feature-annotations.enhanced"
  cmp "$vector_dir/enhanced.bin" "$vector_dir/pre-filter-annotations.enhanced"

  CHICAGO_FEATURE_ENHANCED="$vector_dir/enhanced.bin" \
  CHICAGO_FEATURE_SOURCE="$vector_dir/source.bin" \
  CHICAGO_FEATURE_SCALE_DIR="$vector_dir" \
    "$native_test" \
      -p /gdix51c0/chicago-feature/matches-oracle-scale-space >/dev/null
  CHICAGO_FEATURE_ENHANCED="$vector_dir/enhanced.bin" \
  CHICAGO_FEATURE_EXTREMA="$vector_dir/extrema.bin" \
    "$native_test" \
      -p /gdix51c0/chicago-feature/matches-oracle-extrema >/dev/null
  CHICAGO_FEATURE_ENHANCED="$vector_dir/enhanced.bin" \
  CHICAGO_FEATURE_REFINEMENT="$vector_dir/refinement.bin" \
    "$native_test" \
      -p /gdix51c0/chicago-feature/matches-oracle-refinement >/dev/null
  CHICAGO_FEATURE_ENHANCED="$vector_dir/enhanced.bin" \
  CHICAGO_FEATURE_MATERIALIZATION="$vector_dir/materialization.bin" \
    "$native_test" \
      -p /gdix51c0/chicago-feature/matches-oracle-materialization >/dev/null
  CHICAGO_FEATURE_ENHANCED="$vector_dir/enhanced.bin" \
  CHICAGO_FEATURE_MATERIALIZATION="$vector_dir/materialization.bin" \
  CHICAGO_FEATURE_DESCRIPTOR="$vector_dir/descriptor.bin" \
    "$native_test" \
      -p /gdix51c0/chicago-feature/matches-oracle-descriptors >/dev/null
  CHICAGO_FEATURE_DESCRIPTOR="$vector_dir/descriptor.bin" \
  CHICAGO_FEATURE_ENHANCED="$vector_dir/enhanced.bin" \
  CHICAGO_FEATURE_FINALIZED="$vector_dir/finalized.bin" \
    "$native_test" \
      -p /gdix51c0/chicago-feature/matches-oracle-finalized-records >/dev/null
  CHICAGO_FEATURE_FINALIZED="$vector_dir/finalized.bin" \
  CHICAGO_FEATURE_MASK="$vector_dir/feature-mask.bin" \
  CHICAGO_FEATURE_POST_MASK="$vector_dir/post-mask.bin" \
  CHICAGO_FEATURE_POST_QUALITY="$vector_dir/post-quality.bin" \
  CHICAGO_FEATURE_ANNOTATIONS="$vector_dir/feature-annotations.bin" \
  CHICAGO_FEATURE_PRE_ANNOTATIONS="$vector_dir/pre-filter-annotations.bin" \
    "$native_test" \
      -p /gdix51c0/chicago-feature/matches-oracle-post-extraction >/dev/null
  print -- "$stem feature-source=exact scales=9/9-exact extrema=exact refinement=exact materialization=exact descriptors=exact finalization=exact feature-mask=exact annotations=exact status-consensus=exact mask-filter=exact partition=exact packing=exact neighbor-score=exact"
done
