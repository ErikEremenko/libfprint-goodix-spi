#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -n "${1:-}" ]; then
  BUILDDIR="$1"
elif [ -n "${LIBFPRINT_BUILD:-}" ]; then
  BUILDDIR="$LIBFPRINT_BUILD"
else
  BUILDDIR="$("$REPO_DIR/scripts/libfprint-build-dir.sh")"
fi

meson test -C "$BUILDDIR" --suite goodix-spi --print-errorlogs
