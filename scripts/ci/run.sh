#!/usr/bin/env bash
# Exercise the shared libfprint builder in a clean checkout, then run tests.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK_PARENT="${CI_WORK_ROOT:-${TMPDIR:-/tmp}}"

[ -d "$WORK_PARENT" ] || install -d "$WORK_PARENT"
WORK_DIR="$(mktemp -d "$WORK_PARENT/goodix-spi-build.XXXXXX")"
SOURCE_DIR="$WORK_DIR/libfprint"
BUILD_DIR="$WORK_DIR/build"
DEVELOPER_BUILD_DIR="$WORK_DIR/build-developer"

"$REPO_DIR/install.sh" --check --skip-hardware-check

LIBFPRINT_CREATE_LINK=0 \
LIBFPRINT_SOURCE_DIR="$SOURCE_DIR" \
  "$REPO_DIR/scripts/fetch-libfprint.sh" >/dev/null

"$REPO_DIR/scripts/libfprint/build.sh" --profile ci \
  "$SOURCE_DIR" "$BUILD_DIR"
"$REPO_DIR/scripts/ci/check-build-profile.sh" release \
  "$BUILD_DIR/libfprint/libfprint-2.so.2.0.0"
"$REPO_DIR/tests/run-driver-tests.sh" "$BUILD_DIR"

GOODIX_SPI_DEVELOPER=1 \
  "$REPO_DIR/scripts/libfprint/build.sh" --profile ci \
  "$SOURCE_DIR" "$DEVELOPER_BUILD_DIR"
"$REPO_DIR/scripts/ci/check-build-profile.sh" developer \
  "$DEVELOPER_BUILD_DIR/libfprint/libfprint-2.so.2.0.0"

printf 'Clean release/developer builds and Goodix SPI test suite passed.\n'
printf 'Build tree: %s\n' "$WORK_DIR"
