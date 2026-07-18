#!/usr/bin/env bash
# Repository-only CI checks that do not require fingerprint hardware.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK_PARENT="${CI_WORK_ROOT:-${TMPDIR:-/tmp}}"

[ -d "$WORK_PARENT" ] || install -d "$WORK_PARENT"
WORK_DIR="$(mktemp -d "$WORK_PARENT/goodix-spi-quality.XXXXXX")"

cd "$REPO_DIR"

EMPTY_TREE="$(git hash-object -t tree /dev/null)"
git diff --check "$EMPTY_TREE"

mapfile -d '' source_files < <(
  git ls-files -z -- '*.c' '*.h'
)
for source_file in "${source_files[@]}"; do
  if ! grep -Fq 'SPDX-License-Identifier: LGPL-2.1-or-later' "$source_file"; then
    printf 'Error: %s has no LGPL-2.1-or-later SPDX identifier.\n' "$source_file" >&2
    exit 1
  fi
  if ! grep -Fqi 'Copyright' "$source_file"; then
    printf 'Error: %s has no copyright notice.\n' "$source_file" >&2
    exit 1
  fi
done

mapfile -d '' shell_files < <(
  find . -path './.git' -prune -o -path './libfprint' -prune -o \
    -type f -name '*.sh' -print0
  find config/install.d -maxdepth 1 -type f -print0
)
bash_files=()
for shell_file in "${shell_files[@]}"; do
  if head -n 1 "$shell_file" | grep -Fq 'zsh'; then
    zsh -n "$shell_file"
  else
    bash -n "$shell_file"
    bash_files+=("$shell_file")
  fi
done
shellcheck --severity=error "${bash_files[@]}"

FPRINTD_SOURCE_DIR="$WORK_DIR/fprintd" \
  "$REPO_DIR/scripts/fetch-fprintd.sh" >/dev/null
git -C "$WORK_DIR/fprintd" apply --check \
  "$REPO_DIR/patches/fprintd/1.94.5-goodix-spi.patch"

printf 'Repository hygiene, shell syntax, licensing, and fprintd patch checks passed.\n'
printf 'Temporary check tree: %s\n' "$WORK_DIR"
