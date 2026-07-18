#!/usr/bin/env bash
# Exercise idle or active-action suspend/resume against live GDIX51C0 hardware.

set -euo pipefail

MODE="${1:-idle}"

case "$MODE" in
  idle|active)
    ;;
  *)
    printf 'Usage: %s [idle|active]\n' "$0" >&2
    exit 2
    ;;
esac

if [ ! -t 0 ]; then
  printf 'Error: suspend testing requires an interactive terminal.\n' >&2
  exit 2
fi

PRE_OUTPUT="$(mktemp)"
ACTIVE_OUTPUT="$(mktemp)"
POST_OUTPUT="$(mktemp)"
POST_RESUME_LOG="$(mktemp)"
LIFECYCLE_LOG="$(mktemp)"
ACTIVE_PID=""

cleanup() {
  if [ -n "$ACTIVE_PID" ] && kill -0 "$ACTIVE_PID" 2>/dev/null; then
    kill "$ACTIVE_PID" 2>/dev/null || true
    wait "$ACTIVE_PID" 2>/dev/null || true
  fi
  rm -f "$PRE_OUTPUT" "$ACTIVE_OUTPUT" "$POST_OUTPUT" "$POST_RESUME_LOG" \
    "$LIFECYCLE_LOG"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

run_verify_match() {
  local output="$1"

  fprintd-verify 2>&1 | tee "$output" || true
  if ! grep -q 'Verify result: verify-match' "$output"; then
    printf 'Error: verification did not match.\n' >&2
    return 1
  fi
}

sudo -v
START_TIME="$(date --iso-8601=seconds)"
PSK_BEFORE="$(sudo sha256sum /var/lib/fprint/gdix51c0.psk 2>/dev/null |
  awk '{print $1}' || true)"

if [ "$MODE" = "idle" ]; then
  printf 'Run the pre-suspend verification.\n\n'
  run_verify_match "$PRE_OUTPUT"
  printf '\nIdle path armed: the action is released and the device remains prewarmed.\n'
else
  printf 'Starting verification with no finger present; it must remain parked until resume.\n'
  fprintd-verify >"$ACTIVE_OUTPUT" 2>&1 &
  ACTIVE_PID="$!"

  ACTION_READY=0
  for _ in $(seq 1 80); do
    if busctl get-property net.reactivated.Fprint \
         /net/reactivated/Fprint/Device/0 \
         net.reactivated.Fprint.Device finger-needed 2>/dev/null |
       grep -q 'true$'; then
      ACTION_READY=1
      break
    fi
    if ! kill -0 "$ACTIVE_PID" 2>/dev/null; then
      break
    fi
    sleep 0.25
  done

  if [ "$ACTION_READY" -ne 1 ]; then
    cat "$ACTIVE_OUTPUT"
    printf 'Error: active verification did not reach the finger-wait boundary.\n' >&2
    exit 1
  fi
  printf 'Active path armed: verification is waiting for FDT-down. Keep your finger off.\n'
fi

printf 'Press Enter to suspend this machine. The script continues after resume. '
read -r _
sudo systemctl suspend
POST_RESUME_TIME="$(date --iso-8601=seconds)"
sleep 2

if [ "$MODE" = "active" ]; then
  for _ in $(seq 1 40); do
    if ! kill -0 "$ACTIVE_PID" 2>/dev/null; then
      break
    fi
    sleep 0.25
  done

  if kill -0 "$ACTIVE_PID" 2>/dev/null; then
    printf 'Error: suspended verification did not cancel after resume.\n' >&2
    exit 1
  fi
  wait "$ACTIVE_PID" 2>/dev/null || true
  ACTIVE_PID=""
  printf '\nCancelled active verification output:\n'
  cat "$ACTIVE_OUTPUT"
fi

printf '\nRun the first post-resume verification.\n\n'
run_verify_match "$POST_OUTPUT"

sudo journalctl -b -u fprintd -o cat --since "$POST_RESUME_TIME" \
  >"$POST_RESUME_LOG"
if [ "$MODE" = "idle" ]; then
  if ! grep -q 'Reopened warm fingerprint device after resume' "$POST_RESUME_LOG"; then
    printf 'Error: idle resume did not reopen and prewarm the device.\n' >&2
    exit 1
  fi
fi

sudo journalctl -b -u fprintd -o cat --since "$START_TIME" \
  >"$LIFECYCLE_LOG"
if [ "$MODE" = "active" ] &&
   ! grep -q 'Suspending active warm fingerprint action' "$LIFECYCLE_LOG"; then
  printf 'Error: active suspend did not invoke the driver suspend callback.\n' >&2
  exit 1
fi

if [ "$MODE" = "active" ] &&
   ! grep -q 'Resuming active warm fingerprint action' "$LIFECYCLE_LOG"; then
  printf 'Error: active resume did not cancel the parked action.\n' >&2
  exit 1
fi

PSK_AFTER="$(sudo sha256sum /var/lib/fprint/gdix51c0.psk 2>/dev/null |
  awk '{print $1}' || true)"
if [ -n "$PSK_BEFORE" ] && [ "$PSK_BEFORE" != "$PSK_AFTER" ]; then
  printf 'Error: persisted PSK changed across suspend/resume.\n' >&2
  exit 1
fi

printf '\nSuspend/resume lifecycle evidence:\n\n'
sudo journalctl -b -u fprintd -o short-precise --since "$START_TIME" |
  grep -E 'Prewarmed fingerprint device|Closed warm fingerprint device|Reopened warm fingerprint device|Suspending active warm fingerprint action|Resuming active warm fingerprint action|error' || true

printf '\nPASS: %s suspend/resume returned to a matching session; persisted PSK unchanged.\n' "$MODE"
