#!/usr/bin/env bash
# Apply the adaptive-print persistence patch to an fprintd source tree.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PATCH_FILE="$REPO_DIR/patches/fprintd/1.94.5-adaptive-persistence.patch"
SOURCE_DIR="${1:-}"

if [ -z "$SOURCE_DIR" ] || [ ! -f "$SOURCE_DIR/src/device.c" ]; then
  printf 'Usage: %s /path/to/fprintd-source\n' "$0" >&2
  exit 2
fi

if git -C "$SOURCE_DIR" apply --check "$PATCH_FILE" 2>/dev/null; then
  git -C "$SOURCE_DIR" apply "$PATCH_FILE"
  printf 'Applied adaptive-print persistence patch to %s\n' "$SOURCE_DIR"
elif grep -q 'verify_print_snapshots' "$SOURCE_DIR/src/device.c" &&
     grep -q 'save_changed_verify_print' "$SOURCE_DIR/src/device.c"; then
  printf 'Adaptive-print persistence patch is already applied in %s\n' "$SOURCE_DIR"
else
  printf 'Error: patch does not apply cleanly to %s.\n' "$SOURCE_DIR" >&2
  printf 'The fprintd verify/identify boundary likely changed; rebase this small patch.\n' >&2
  exit 1
fi
