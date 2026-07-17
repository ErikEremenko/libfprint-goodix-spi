#!/usr/bin/env bash
# Assert that release binaries exclude developer hooks and developer binaries include them.

set -euo pipefail

PROFILE="${1:-}"
LIBRARY="${2:-}"

if [ -z "$PROFILE" ] || [ -z "$LIBRARY" ]; then
  printf 'Usage: %s release|developer LIBFPRINT_LIBRARY\n' "$0" >&2
  exit 2
fi
if [ ! -r "$LIBRARY" ]; then
  printf 'Error: cannot read libfprint library: %s\n' "$LIBRARY" >&2
  exit 2
fi

marker='developer diagnostics and fault injection are compiled in'
case "$PROFILE" in
  release)
    forbidden=(
      "$marker"
      'GDIX51C0_DUMP_FRAMES'
      'GDIX51C0_DUMP_DIR'
      'GDIX51C0_FAULT_'
      '/tmp/gdix51c0_'
      'gdix51c0_frame_'
      'gdix51c0_nofinger_'
      'Chicago sensor id SHA-256'
      'image footer bytes'
      'GDIX51C0_SPI_SPEED_HZ'
      'GDIX51C0_DAC_FROM_OTP'
      'GDIX51C0_REG0082'
      'GDIX51C0_REG005C'
      'GDIX51C0_PRIME_T0'
      'GDIX51C0_FDT_DYNAMIC'
      'GDIX51C0_FDT_DOWN_BIAS'
      'GDIX51C0_FDT_UP_NUM'
      'GDIX51C0_CAPTURE_SETTLE_MS'
      'GDIX51C0_CAPTURE_NAV_DATA'
      'GDIX51C0_FORCE_DARK_FRAME'
      'GDIX51C0_ENABLE_DARK_FRAME'
    )
    for text in "${forbidden[@]}"; do
      if grep -aFq "$text" "$LIBRARY"; then
        printf 'Error: release libfprint contains developer-only marker: %s\n' "$text" >&2
        exit 1
      fi
    done
    ;;
  developer)
    for text in "$marker" 'GDIX51C0_DUMP_FRAMES' 'GDIX51C0_FAULT_'; do
      if ! grep -aFq "$text" "$LIBRARY"; then
        printf 'Error: developer libfprint is missing expected hook marker: %s\n' "$text" >&2
        exit 1
      fi
    done
    ;;
  *)
    printf 'Error: profile must be release or developer.\n' >&2
    exit 2
    ;;
esac

printf '%s binary profile verified: %s\n' "$PROFILE" "$LIBRARY"
