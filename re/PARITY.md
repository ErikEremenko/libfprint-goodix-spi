# GDIX51C0 official-driver parity

This is the current reverse-engineering record for the native Linux driver.
The reference is the installed Windows `gfspi.dll` + `EngineAdapter.dll` +
`AlgoChicago.dll` stack selected by chip `0x2504`, sensor type 12
(`ChicagoHS`). Historical investigation notebooks were removed after their
verified conclusions were incorporated here and into tests.

## Current status

| Boundary | Native implementation | Evidence | Remaining scope |
| --- | --- | --- | --- |
| Sensor identity | Chip response, 80x64 geometry, 64-byte OTP and ChicagoHS profile are validated before calibration | Live hardware | Add profiles only from equivalent official selection evidence |
| SPI command transport | Exact-length full-duplex reads, command-specific ACK/data modes and nested retries | Deterministic fault injection and hardware timing | Retain as regression |
| TLS and PSK | OpenSSL PSK server, WB provisioning, persisted key, guarded stale-key repair | Provision, hash verification, TLS and injected stale-key hardware tests | Test a naturally Windows-rotated key when convenient |
| Capture/FDT | Initial `0x22` image, same-press `0x20` retries, 5/6-zone policy, FDT-up between completed stages | Live match/no-match and held-finger traces | Other sensor-type policies are unknown |
| Calibration/ImageBase | Official first-run map generation, sensor binding, CRCs, temporal state and cold-open rebase | Byte-exact Wine replay and hardware restart/suspend | Legacy-DAC first-run profile is not covered |
| Preprocessing | Raw validation, source plane, candidates, quality/coverage and `0x29aa`/`0x7531` policy | Historical finger/no-finger replay is exact | Preserve real bad-capture cases as regressions |
| Enrollment | Official 12 accepted-stage position policy and first-tip lifecycle | Captured 17-attempt sequence and template comparison | Account/test-mode branches are unused |
| Template format | V1 wrapper, Chicago inner records, relation forest and CRC | Byte-exact initialized data and round trips | V2 is not implemented without a real consumer |
| Identify | Type-24 scheduler, candidate filters, transformed metrics, score and three-capture retry | Wine differential vectors and live matches/no-matches | Public callback distinctions can still be refined |
| `templateStudy` | Under-capacity append and capacity-50 replacement with selected-source handoff | Byte-exact oracles; 46 persisted hardware updates and restart | Retain durability tests |
| Lifecycle | Cold-open refresh, lift wait, cancellation, idle and active suspend/resume | Both hardware suspend/resume gates pass | Retain as regression |

The implementation intentionally zeroes unused template padding instead of
reproducing the Windows wrapper's uninitialized-memory disclosure. This is the
only known deliberate byte-level safety deviation with no matching semantics.

## Transport timing contract

The official `SpiSendDataToDevice` family establishes these boundaries:

- sleep 1 ms before the initial submit;
- nonzero ACK/data deadlines below 1000 ms are normalized to 1 second;
- resend the same command immediately once after ACK timeout;
- on required-data timeout, resubmit the complete lower command once;
- command low nibble 3 uses a 10-second data window; FDT-manual follows it;
- NAV and image data use 1-second windows;
- get-EVK has three outer attempts, with a hard reset and 500 ms delay between
  exhausted attempts;
- post-TLS D4 success is mandatory; exhaustion restarts activation;
- exhausting both T0 image attempts is terminal to that activation, because
  later FDT ACKs do not prove the image state recovered.

ACK and response packets may be queued back-to-back in the same IRQ-high
window. Reads therefore clock exactly the advertised packet length; historical
tail over-read experiments consumed the next header and were incorrect.

`scripts/gdix51c0/test-spi-retry.sh` injects failures only when explicitly
armed. The normal driver does not drop packets or add the test delays.

## Session and PSK

Windows uses TLS-PSK with `PSK-AES128-GCM-SHA256`. The host's 32-byte outer PSK
is used directly by TLS. A 102-byte WB container transports a chosen key to MCU
register `0xbb010003`; its integrity is confirmed through `SHA-256(WB)` at
`0xbb020003`. The wrap reduces to standard SHA-256, AES-256-GCM, and
HMAC-SHA256 with vendor constants implemented in `gdix51c0-wb.c`.

`0xbb010003` is confirmed rewritable but read-empty, including immediately
after a verified write. Linux therefore stores its generated plaintext key in
a root-owned `0600` file. A stale key is repaired only after two independent
complete client flights fail at TLS authentication; missing IRQs, transport
timeouts, or an incomplete handshake cannot trigger provisioning. Repair
rewrites the same persisted key once per device-open rather than rotating it.

Committed WB tests use the synthetic PSK `00 01 ... 1f`. No captured key or
unit OTP belongs in the repository.

## Capture and calibration

The packed stream contains 5120 12-bit samples. `ChicagoHUDataRegroup` maps it
to the canonical 64x80 Chicago input. OTP supplies the per-unit DAC values;
hard-coded prototype values caused the early low-quality captures.

On a cold hardware handle, finger-off FDT measurements guard a T0 no-finger
capture. The driver reproduces the official Q13-unity first-run gain map,
prepared ImageBase offset map, state fields, CRCs, and version marker. Later
ImageBase refresh preserves learned calibration state while replacing the base
plane. If finger-off state is not proven, refresh is deferred until FDT-up.

## Native Chicago backend

The I/O-free backend under `lib/goodix/chicago/` owns calibration validation,
preprocessing, feature construction, enrollment, templates, identify, and
adaptive learning. It does not load Windows DLLs at runtime.

Key recovered behavior:

- preprocessing maintains the official temporal gain and local adaptive state;
- poor-capture decisions reproduce both early `0x29aa` validation and the
  mode-24 `0x7531` contrast policy;
- enrollment completes after 12 policy-accepted samples while retaining
  position-rejected algorithm records;
- identify retries at most three images on the same request and preserves an
  earlier valid no-match if a later capture is bad;
- successful eligible matches update only the selected gallery template;
- templates grow to 50 subtemplates, then use the official replacement path;
- the separate fprintd patch saves a print only after a successful match and
  only if its serialized bytes changed.

## Oracle and private data policy

Wine harnesses in `re/algo-oracle/` load the original DLLs only as development
oracles. They require a private 64-byte OTP fixture through `ENGINE_OTP_FILE`.
Raw captures, calibration, ImageBase, templates, DLLs, logs, crash dumps, and
OTP/PSK state are ignored and must not be committed.

The public source keeps only protocol constants needed for interoperability,
synthetic deterministic vectors, and the code required to reproduce a result.
Private fixtures can enable additional oracle-backed tests; without them the
deterministic native suite remains runnable.
