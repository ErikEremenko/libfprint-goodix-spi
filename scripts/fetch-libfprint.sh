#!/usr/bin/env bash
# Fetch the pinned upstream libfprint source into a user cache.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LIBFPRINT_REPOSITORY="${LIBFPRINT_REPOSITORY:-https://gitlab.freedesktop.org/libfprint/libfprint.git}"
LIBFPRINT_REF="${LIBFPRINT_REF:-v1.94.10}"
CACHE_ROOT="${LIBFPRINT_CACHE_ROOT:-${XDG_CACHE_HOME:-$HOME/.cache}/goodix-spi-driver/sources}"
LIBFPRINT_LINK="${LIBFPRINT_LINK:-$REPO_DIR/libfprint}"
LIBFPRINT_CREATE_LINK="${LIBFPRINT_CREATE_LINK:-1}"
INTEGRATION_REVISION="${GOODIX_SPI_INTEGRATION_REVISION:-2}"
SAFE_REF="$(printf '%s' "$LIBFPRINT_REF" | sed 's/[^A-Za-z0-9._-]/_/g')"
DEFAULT_SOURCE_DIR="${LIBFPRINT_SOURCE_DIR:-$CACHE_ROOT/libfprint-$SAFE_REF-integration-$INTEGRATION_REVISION}"

usage() {
  printf 'Usage: %s [destination]\n' "$0"
  printf '\nFetches %s from:\n  %s\n' "$LIBFPRINT_REF" "$LIBFPRINT_REPOSITORY"
  printf 'Default destination:\n  %s\n' "$DEFAULT_SOURCE_DIR"
  printf '\nEnvironment overrides: LIBFPRINT_REPOSITORY, LIBFPRINT_REF,\n'
  printf 'LIBFPRINT_CACHE_ROOT, LIBFPRINT_SOURCE_DIR, LIBFPRINT_LINK,\n'
  printf 'LIBFPRINT_CREATE_LINK (set to 0 to disable the convenience symlink),\n'
  printf 'GOODIX_SPI_INTEGRATION_REVISION.\n'
}

link_checkout() {
  if [ "$LIBFPRINT_CREATE_LINK" = "0" ]; then
    return
  fi

  if [ -e "$LIBFPRINT_LINK" ] && [ ! -L "$LIBFPRINT_LINK" ]; then
    printf 'Error: refusing to replace non-symlink path %s.\n' "$LIBFPRINT_LINK" >&2
    exit 1
  fi

  ln -sfn "$SOURCE_DIR" "$LIBFPRINT_LINK"
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
  if [ "$STORED_REF" != "$LIBFPRINT_REF" ]; then
    printf 'Error: cached checkout %s was created for ref %s, not %s.\n' \
      "$SOURCE_DIR" "${STORED_REF:-unknown}" "$LIBFPRINT_REF" >&2
    printf 'Choose another destination or set LIBFPRINT_SOURCE_DIR.\n' >&2
    exit 1
  fi
  STORED_INTEGRATION="$(git -C "$SOURCE_DIR" config --get goodix-spi.integrationRevision || true)"
  if [ "$STORED_INTEGRATION" != "$INTEGRATION_REVISION" ]; then
    printf 'Error: cached checkout %s uses integration revision %s, not %s.\n' \
      "$SOURCE_DIR" "${STORED_INTEGRATION:-unknown}" "$INTEGRATION_REVISION" >&2
    printf 'Choose another destination or remove that cache after preserving local work.\n' >&2
    exit 1
  fi

  link_checkout
  printf 'Using cached libfprint %s at %s\n' "$LIBFPRINT_REF" "$SOURCE_DIR" >&2
  printf '%s\n' "$SOURCE_DIR"
  exit 0
fi

install -d "$(dirname "$SOURCE_DIR")"
TEMP_DIR="$SOURCE_DIR.fetching.$$"
cleanup() {
  rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

printf 'Fetching libfprint %s from %s\n' "$LIBFPRINT_REF" "$LIBFPRINT_REPOSITORY" >&2
git init --quiet "$TEMP_DIR"
git -C "$TEMP_DIR" remote add origin "$LIBFPRINT_REPOSITORY"
git -C "$TEMP_DIR" fetch --quiet --depth=1 origin "$LIBFPRINT_REF"
git -C "$TEMP_DIR" checkout --quiet --detach FETCH_HEAD
git -C "$TEMP_DIR" config goodix-spi.sourceRef "$LIBFPRINT_REF"
git -C "$TEMP_DIR" config goodix-spi.integrationRevision "$INTEGRATION_REVISION"
mv "$TEMP_DIR" "$SOURCE_DIR"
trap - EXIT

link_checkout
printf '%s\n' "$SOURCE_DIR"
