#!/usr/bin/env bash
# Patch, build, and shadow-install fprintd for adaptive print persistence.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ID="goodix-spi-driver"
OWNED_MARKER="# Managed by $PROJECT_ID; do not edit."
SOURCE_DIR="${1:-${FPRINTD_SOURCE_DIR:-}}"
LIBFPRINT_BUILD="${LIBFPRINT_BUILD:-}"
BUILDDIR="${FPRINTD_BUILDDIR:-}"
PROJECT_INSTALL_ROOT="${PROJECT_INSTALL_ROOT:-/opt/$PROJECT_ID}"
INSTALL_DIR="${FPRINTD_INSTALL_DIR:-$PROJECT_INSTALL_ROOT/fprintd}"
LIBFPRINT_INSTALL_DIR="${LIBFPRINT_INSTALL_DIR:-$PROJECT_INSTALL_ROOT/libfprint}"
DROPIN_DIR="${FPRINTD_DROPIN_DIR:-/etc/systemd/system/fprintd.service.d}"
DROPIN="${FPRINTD_DROPIN:-$DROPIN_DIR/30-goodix-spi-adaptive.conf}"
RESTART="${FPRINTD_RESTART:-1}"

run_root() {
  if [ "${EUID:-$(id -u)}" -eq 0 ]; then "$@"; else sudo "$@"; fi
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  printf 'Usage: %s [path/to/fprintd-source]\n' "$0"
  exit 0
fi
[ "$#" -le 1 ] || { printf 'Usage: %s [path/to/fprintd-source]\n' "$0" >&2; exit 2; }

if [ -z "$SOURCE_DIR" ]; then
  SOURCE_DIR="$("$SCRIPT_DIR/fetch-fprintd.sh")"
fi
if [ -z "$BUILDDIR" ]; then
  BUILDDIR="$SOURCE_DIR/build-goodix-spi"
fi
if [ ! -f "$SOURCE_DIR/meson.build" ] || [ ! -f "$SOURCE_DIR/src/device.c" ]; then
  printf 'Error: %s does not look like an fprintd source tree.\n' "$SOURCE_DIR" >&2
  exit 2
fi
if [ -z "$LIBFPRINT_BUILD" ]; then
  LIBFPRINT_BUILD="$("$REPO_DIR/scripts/libfprint-build-dir.sh")"
fi

LIBFPRINT_PC="$LIBFPRINT_BUILD/meson-uninstalled/libfprint-2-uninstalled.pc"
[ -f "$LIBFPRINT_PC" ] || {
  printf 'Error: libfprint build metadata was not found at %s\n' "$LIBFPRINT_PC" >&2
  exit 1
}

"$SCRIPT_DIR/apply-fprintd-patch.sh" "$SOURCE_DIR"
export PKG_CONFIG_PATH="$LIBFPRINT_BUILD/meson-uninstalled${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
if [ -f "$BUILDDIR/meson-private/coredata.dat" ]; then
  meson setup "$BUILDDIR" "$SOURCE_DIR" --reconfigure \
    --prefix=/usr -Dman=false -Dgtk_doc=false -Dpam=false -Dsystemd=false
else
  meson setup "$BUILDDIR" "$SOURCE_DIR" \
    --prefix=/usr -Dman=false -Dgtk_doc=false -Dpam=false -Dsystemd=false
fi
meson compile -C "$BUILDDIR" fprintd

BUILT_DAEMON="$BUILDDIR/src/fprintd"
[ -x "$BUILT_DAEMON" ] || { printf 'Error: fprintd was not built.\n' >&2; exit 1; }

if run_root test -e "$DROPIN"; then
  FIRST="$(run_root head -n 1 "$DROPIN" 2>/dev/null || true)"
  if [ "$FIRST" != "$OWNED_MARKER" ]; then
    printf 'Error: refusing to overwrite non-project file: %s\n' "$DROPIN" >&2
    exit 1
  fi
fi

DROPIN_TMP="$(mktemp)"
trap 'rm -f "$DROPIN_TMP"' EXIT
{
  printf '%s\n' "$OWNED_MARKER"
  printf '[Service]\n'
  printf 'ExecStart=\n'
  printf 'ExecStart=%s/fprintd\n' "$INSTALL_DIR"
  printf 'Environment=LD_LIBRARY_PATH=%s\n' "$LIBFPRINT_INSTALL_DIR"
} > "$DROPIN_TMP"

run_root install -d "$INSTALL_DIR" "$DROPIN_DIR"
run_root install -m 0755 "$BUILT_DAEMON" "$INSTALL_DIR/fprintd"
run_root install -m 0644 "$DROPIN_TMP" "$DROPIN"
if [ "$RESTART" != "0" ]; then
  run_root systemctl daemon-reload
  run_root systemctl restart fprintd.service
fi

printf 'Installed adaptive fprintd at %s/fprintd\n' "$INSTALL_DIR"
