# Reverse-engineering workspace

This directory contains source-only interoperability research for the Goodix
driver. It intentionally excludes proprietary binaries, captured fingerprint
frames, calibration files, crash dumps, logs, active credentials, and
unit-specific sensor data.

## Private fixtures

Wine EngineAdapter probes require the physical sensor's 64-byte OTP. Store it
locally as:

```text
re/private/gdix51c0-otp.bin
```

`re/private/` is ignored. The engine scripts pass that file to Wine through
`ENGINE_OTP_FILE`; the oracle refuses to attach without an exact 64-byte
fixture. Do not commit the OTP because it contains a stable unit serial and
factory calibration.

The same rule applies to DLLs, raw images, templates, ImageBase, calibration,
PSK state, and generated oracle output. The ignore rules are a guard, not a
substitute for reviewing `git diff --cached` before publishing.

## Documentation

`PARITY.md` is the maintained official-versus-native contract for transport,
security, capture, calibration, matching, persistence, and lifecycle behavior.
Chronological investigation notebooks are intentionally not retained in the
public tree; verified conclusions belong in `PARITY.md`, production comments,
or deterministic tests.

The recovered WB wrapping constants remain in the public production source
because Linux provisioning cannot work without them. They are vendor protocol
constants, not a per-device PSK. All committed WB self-tests use the explicit
synthetic PSK `00 01 ... 1f`.
