#!/usr/bin/env bash
# Resolve the build directory for the cached or explicitly selected libfprint.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ -n "${LIBFPRINT_BUILD:-}" ]; then
  printf '%s\n' "$LIBFPRINT_BUILD"
else
  LIBFPRINT_SOURCE="$("$SCRIPT_DIR/fetch-libfprint.sh")"
  printf '%s/builddir\n' "$LIBFPRINT_SOURCE"
fi
