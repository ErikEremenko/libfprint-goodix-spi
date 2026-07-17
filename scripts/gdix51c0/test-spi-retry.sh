#!/usr/bin/env bash
# Exercise one official SPI retry boundary against live hardware.

set -euo pipefail

MODE="${1:-response}"
DROPIN_NAME="90-gdix51c0-spi-retry-test.conf"
DROPIN_PATH="/run/systemd/system/fprintd.service.d/$DROPIN_NAME"
INSTALLED_LIB="${GOODIX_SPI_TEST_LIB:-/opt/goodix-spi-driver/libfprint/libfprint-2.so.2.0.0}"

case "$MODE" in
  ack)
    FAULT_ENV="GDIX51C0_FAULT_ACK_TIMEOUT_ONCE=FDT-down"
    EXPECTED="FDT-down ACK timed out; immediate same-command resend"
    NOTE="The ACK test deliberately consumes one 1 s FDT-down ACK window."
    ;;
  response)
    FAULT_ENV="GDIX51C0_FAULT_RESPONSE_TIMEOUT_ONCE=FDT-manual"
    EXPECTED="FDT-manual data response timed out; resubmitting complete command once"
    NOTE="The response test deliberately consumes the 10 s FDT-manual data window."
    ;;
  d4)
    FAULT_ENV="GDIX51C0_FAULT_SYNC_ACK_EXHAUST_ONCE=post-tls-d4"
    EXPECTED="mandatory post-TLS D4 failed, then activation attempt 2 recovers"
    NOTE="The D4 test deliberately consumes both 1 s ACK attempts before reactivation."
    ;;
  t0)
    FAULT_ENV="GDIX51C0_FAULT_T0_DATA_EXHAUST_ONCE=1"
    EXPECTED="both T0 data attempts fail, then activation attempt 2 captures T0 and matches"
    NOTE="The T0 test consumes both 1 s image-data windows; keep your finger off until prompted."
    ;;
  evk)
    FAULT_ENV="GDIX51C0_FAULT_EVK_FAILURES_ONCE=2"
    EXPECTED="EVK outer attempts 1 and 2 hard-reset, attempt 3 succeeds without activation recovery"
    NOTE="The EVK test consumes four 1 s data windows; keep your finger off until prompted."
    ;;
  psk)
    FAULT_ENV="GDIX51C0_FAULT_PSK_MISMATCH_ONCE=1"
    EXPECTED="confirmed stale PSK schedules one repair; activation attempt 2 restores the persisted key and matches"
    NOTE="The PSK test alters one in-memory handshake key, then rewrites the same persisted Linux key once; it does not rotate or expose the key."
    ;;
  psk-budget)
    FAULT_ENV="GDIX51C0_FAULT_SYNC_ACK_EXHAUST_ONCE=post-tls-d4 GDIX51C0_FAULT_PSK_MISMATCH_ONCE=2 GDIX51C0_FAULT_T0_DATA_EXHAUST_ONCE=1"
    EXPECTED="one pre-PSK activation failure, stale-key restoration, failed T0, a fresh bounded activation budget, then MATCH"
    NOTE="The compound test uses a clean terminal D4 failure before PSK recovery and rewrites only the unchanged persisted Linux key."
    ;;
  *)
    printf 'Usage: %s [ack|response|d4|t0|evk|psk|psk-budget]\n' "$0" >&2
    exit 2
    ;;
esac

if [ ! -r "$INSTALLED_LIB" ] ||
   ! grep -aFq 'developer diagnostics and fault injection are compiled in' "$INSTALLED_LIB"; then
  printf 'Error: %s is not a developer build.\n' "$INSTALLED_LIB" >&2
  printf 'Reinstall with GOODIX_SPI_DEVELOPER=1 before running fault-injection tests.\n' >&2
  exit 2
fi

cleanup() {
  trap - EXIT INT TERM
  sudo rm -f "$DROPIN_PATH"
  sudo systemctl daemon-reload
  sudo systemctl restart fprintd
  printf '\nRemoved the runtime fault injection and restarted fprintd.\n'
}

cleanup_after_signal() {
  cleanup
  exit 130
}

sudo -v
trap cleanup EXIT
trap cleanup_after_signal INT TERM

START_TIME="$(date --iso-8601=seconds)"
printf '[Service]\nEnvironment=%s\n' "$FAULT_ENV" |
  sudo systemctl edit --runtime --drop-in="$DROPIN_NAME" --stdin fprintd.service
sudo systemctl restart fprintd

printf 'Armed: %s\n' "$FAULT_ENV"
printf '%s\n' "$NOTE"
printf 'Run one physical verification now. The temporary setting is removed afterward.\n\n'

fprintd-verify || true

printf '\nExpected recovery: %s\n\n' "$EXPECTED"
sudo journalctl -u fprintd -o short-precise --since "$START_TIME" |
  grep -E 'armed one-shot|armed required-data|fault injection dropped|ACK timed out|ACK attempt|data response timed out|response wait failed|get-EVK attempt|get-evk-version required response read|image capture attempt|image read failed|T0 capture failed|mandatory post-TLS D4 failed|SSL_accept|PSK|authentication mismatch|auto-restoring|provision|fresh .*recovery budget|activation failed|TLS handshake complete|init calibration pass done|Chicago (verify|identify) score|error' || true
