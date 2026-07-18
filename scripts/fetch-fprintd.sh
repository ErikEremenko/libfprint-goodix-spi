#!/usr/bin/env bash
# Fetch the fprintd release supported by the Goodix SPI integration patch.

set -euo pipefail

FPRINTD_REPOSITORY="${FPRINTD_REPOSITORY:-https://gitlab.freedesktop.org/libfprint/fprintd.git}"
FPRINTD_REF="${FPRINTD_REF:-v1.94.5}"
CACHE_ROOT="${FPRINTD_CACHE_ROOT:-${XDG_CACHE_HOME:-$HOME/.cache}/goodix-spi-driver/sources}"
SAFE_REF="$(printf '%s' "$FPRINTD_REF" | sed 's/[^A-Za-z0-9._-]/_/g')"
DEFAULT_SOURCE_DIR="${FPRINTD_SOURCE_DIR:-$CACHE_ROOT/fprintd-$SAFE_REF}"

usage() {
  printf 'Usage: %s [destination]\n' "$0"
  printf '\nFetches %s from:\n  %s\n' "$FPRINTD_REF" "$FPRINTD_REPOSITORY"
  printf 'Default destination:\n  %s\n' "$DEFAULT_SOURCE_DIR"
  printf '\nEnvironment overrides: FPRINTD_REPOSITORY, FPRINTD_REF,\n'
  printf 'FPRINTD_CACHE_ROOT, FPRINTD_SOURCE_DIR.\n'
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

if [ "$#" -gt 1 ]; then
  usage >&2
  exit 2
fi

SOURCE_DIR="${1:-$DEFAULT_SOURCE_DIR}"

if [ -e "$SOURCE_DIR" ]; then
  if ! git -C "$SOURCE_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    printf 'Error: %s exists but is not a Git checkout.\n' "$SOURCE_DIR" >&2
    exit 1
  fi

  STORED_REF="$(git -C "$SOURCE_DIR" config --get goodix-spi.sourceRef || true)"
  if [ "$STORED_REF" != "$FPRINTD_REF" ]; then
    printf 'Error: cached checkout %s was created for ref %s, not %s.\n' \
      "$SOURCE_DIR" "${STORED_REF:-unknown}" "$FPRINTD_REF" >&2
    printf 'Choose another destination or set FPRINTD_SOURCE_DIR.\n' >&2
    exit 1
  fi

  printf 'Using cached fprintd %s at %s\n' "$FPRINTD_REF" "$SOURCE_DIR" >&2
  printf '%s\n' "$SOURCE_DIR"
  exit 0
fi

install -d "$(dirname "$SOURCE_DIR")"
TEMP_DIR="$SOURCE_DIR.fetching.$$"
cleanup() {
  rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

printf 'Fetching fprintd %s from %s\n' "$FPRINTD_REF" "$FPRINTD_REPOSITORY" >&2
git init --quiet "$TEMP_DIR"
git -C "$TEMP_DIR" remote add origin "$FPRINTD_REPOSITORY"
git -C "$TEMP_DIR" fetch --quiet --depth=1 origin "$FPRINTD_REF"
git -C "$TEMP_DIR" checkout --quiet --detach FETCH_HEAD
git -C "$TEMP_DIR" config goodix-spi.sourceRef "$FPRINTD_REF"
mv "$TEMP_DIR" "$SOURCE_DIR"
trap - EXIT

printf '%s\n' "$SOURCE_DIR"
