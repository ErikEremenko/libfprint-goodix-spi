#!/usr/bin/env bash
# Remove only files recorded by the Goodix SPI driver installer.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ID="goodix-spi-driver"
OWNED_MARKER="# Managed by $PROJECT_ID; do not edit."
PROJECT_INSTALL_ROOT="${PROJECT_INSTALL_ROOT:-/opt/$PROJECT_ID}"
STATE_DIR="${GOODIX_SPI_STATE_DIR:-/var/lib/$PROJECT_ID}"
MANIFEST_FILE="${GOODIX_SPI_INSTALL_MANIFEST:-$STATE_DIR/install.manifest}"
FPRINTD_DROPIN_DIR="${FPRINTD_DROPIN_DIR:-/etc/systemd/system/fprintd.service.d}"
FPRINTD_DROPIN="${GOODIX_SPI_LIBFPRINT_DROPIN:-$FPRINTD_DROPIN_DIR/20-goodix-spi-libfprint.conf}"
FPRINTD_ADAPTIVE_DROPIN="${GOODIX_SPI_FPRINTD_DROPIN:-$FPRINTD_DROPIN_DIR/30-goodix-spi-adaptive.conf}"
UDEV_RULE_DST="${GOODIX_SPI_UDEV_RULE:-/etc/udev/rules.d/70-libfprint-2.rules}"
DEVICE_METADATA="$SCRIPT_DIR/config/supported-spi-devices.tsv"
PURGE=0

case "${1:-}" in
  "") ;;
  --purge) PURGE=1 ;;
  -h|--help)
    printf 'Usage: %s [--purge]\n' "$0"
    printf 'Without --purge, sensor keys/calibration and enrolled prints are retained.\n'
    exit 0
    ;;
  *) printf 'Usage: %s [--purge]\n' "$0" >&2; exit 2 ;;
esac

run_root() {
  if [ "${EUID:-$(id -u)}" -eq 0 ]; then "$@"; else sudo "$@"; fi
}

remove_manifest_files() {
  local first path
  if ! run_root test -f "$MANIFEST_FILE"; then
    printf 'No install manifest found at %s; nothing was removed.\n' "$MANIFEST_FILE"
    return 0
  fi
  first="$(run_root head -n 1 "$MANIFEST_FILE")"
  if [ "$first" != "$OWNED_MARKER" ]; then
    printf 'Error: install manifest is not owned by this project: %s\n' "$MANIFEST_FILE" >&2
    return 1
  fi

  while IFS= read -r path; do
    [ -n "$path" ] || continue
    case "$path" in \#*) continue ;; esac
    case "$path" in
      "$PROJECT_INSTALL_ROOT"/*|"$FPRINTD_DROPIN"|"$FPRINTD_ADAPTIVE_DROPIN"|"$UDEV_RULE_DST")
        printf 'Removing %s\n' "$path"
        run_root rm -f -- "$path"
        ;;
      *)
        printf 'Error: unsafe path in install manifest: %s\n' "$path" >&2
        return 1
        ;;
    esac
  done < <(run_root cat "$MANIFEST_FILE")

  run_root rm -f "$MANIFEST_FILE"
  run_root rmdir \
    "$PROJECT_INSTALL_ROOT/libfprint" \
    "$PROJECT_INSTALL_ROOT/fprintd" \
    "$PROJECT_INSTALL_ROOT" \
    "$STATE_DIR" 2>/dev/null || true
}

purge_device_state() {
  local driver acpi hook extra path
  while read -r driver acpi hook extra; do
    [ -z "${driver:-}" ] && continue
    case "$driver" in \#*) continue ;; esac
    [ -x "$SCRIPT_DIR/config/install.d/$hook" ] || continue
    while IFS= read -r path; do
      case "$path" in
        /var/lib/fprint/*)
          printf 'Purging %s\n' "$path"
          run_root rm -f -- "$path"
          ;;
        *)
          printf 'Error: unsafe purge path from %s: %s\n' "$hook" "$path" >&2
          return 1
          ;;
      esac
    done < <("$SCRIPT_DIR/config/install.d/$hook" purge)
  done < "$DEVICE_METADATA"
}

remove_manifest_files
[ "$PURGE" -eq 0 ] || purge_device_state

if command -v udevadm >/dev/null 2>&1; then
  run_root udevadm control --reload-rules || true
fi
if command -v systemctl >/dev/null 2>&1; then
  run_root systemctl daemon-reload
  run_root systemctl restart fprintd.service 2>/dev/null || true
fi

printf 'Goodix SPI shadow installation removed. Enrolled prints were retained.\n'
