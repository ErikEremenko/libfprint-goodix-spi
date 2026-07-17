# Architecture

The project is organized around observed protocol boundaries, not the Goodix
brand name. Sharing code requires evidence from at least two implementations
that the contract is identical.

## Current layers

### `lib/goodix/common/`

This private source module contains only primitives independently demonstrated
by more than one Goodix implementation. Its Meson manifest contributes sources
to libfprint's internal driver library; it is not installed and exposes no
public ABI.

`goodix-wire.*` is used by GDIX51C0 and implements framing and checksum
behavior independently observed in the GoodixTLS 5110 and 53x5
implementations used as extraction evidence. Those external implementations
do not consume this source API, and their sensors are not supported by this
repository. The implementations use the same transport-neutral inner byte
contract:

- a four-byte outer header containing flags, little-endian payload length and
  the header-byte sum where that outer wrapper is present;
- a three-byte command header containing command and little-endian length;
- a final checksum that makes the command packet sum to `0xaa`;
- the observed `0x88` null-checksum exception;
- a command identity composed from a four-bit category and three-bit operation.

`goodix-command.*` names the command IDs and explicitly serializes the fixed
idle, reset, sensor-register, powerdown, chip-enable and PSK/storage payloads
shared by GDIX51C0 and GoodixTLS. It does not schedule requests or interpret
device-specific response timing.

`goodix-sensor-profile.*` is the transport-neutral identity map. The first
entry binds live chip ID `0x2504` to official sensor type 12, ChicagoHS,
geometry 80x64 and a 64-byte OTP. Device drivers still own the register read
and reject profiles incompatible with their transport or backend.

`goodix-mcu-config.*` implements the shared `0xa5a5` little-endian word-sum
checksum used by Goodix MCU configuration blobs. GDIX51C0 still owns the
configuration contents and all OTP-derived register changes.

`goodix-image.*` decodes the unusual six-byte/four-pixel 12-bit packing used by
all three implementations. It retains stream order. GDIX51C0's
`ChicagoHUDataRegroup`, the 5110's 88-to-64-column crop and 53x5 geometry remain
in their device layers.

`goodix-crc.*` implements CRC-32/MPEG-2, used by the GDIX51C0 Chicago system
template and the 53x5 GTLS/GEA stack. The GDIX Windows wrapper's accidental
16-bit input-length truncation remains in its device-specific caller.

None of these helpers performs I/O or owns packet scheduling.

### `drivers/gdix51c0/gdix51c0-proto.*`

Owns SPI transactions, chip-select boundaries, idle-header handling and the
GDIX51C0 command wrappers. It delegates byte and fixed-payload construction to
`goodix-wire` and `goodix-command` but keeps transport behavior device-specific.

### GDIX51C0 session and capture

The remaining protocol, listener, TLS BIO, PSK provisioning, WB registers,
GPIO reset/IRQ, FDT, DAC and raw-image capture code is specific to this sensor
until another SPI device proves an identical contract.

### `lib/goodix/chicago/`

Calibration, preprocessing, features, enrollment, template serialization,
matching and `templateStudy` are tied to the recovered ChicagoHS sensor-type-12
behavior. They form an I/O-free biometric backend and are not generic Goodix
APIs. GDIX51C0 supplies its sensor-bound calibration, ImageBase and regrouped
80x64 raw frame to this backend. Like common, Chicago is compiled as a private
source module rather than installed as a public or independently versioned
library.

### fprintd persistence

libfprint mutates a matched print in memory after eligible adaptive learning.
The separate fprintd patch writes that changed serialization back only after a
successful match. This remains independent of repository branding.

The patch snapshots candidate serializations before verify/identify and saves
only the matched print when its bytes changed. Non-adaptive matchers do not
cause writes. It targets fprintd `v1.94.5` and is installed by default;
`./install.sh --without-fprintd` is the explicit opt-out. Libfprint upgrades
do not require rebasing it unless fprintd's ownership or callback lifecycle
also changes.

## Matcher backend policy

A shared Goodix command protocol does not imply a shared biometric algorithm.
GDIX51C0 is confirmed as ChicagoHS with the type-24 identify mode, and the
complete native backend is implemented. GoodixTLS 5110 has compatible-looking
output geometry but no official backend-selection proof. The checked Goodix
53x5 reference implementation has a distinct 108x88 GTLS/GEA path and uses
SIGFM, but this repository retains no official matcher-selection evidence for
that sensor family. It therefore cannot be assumed to use Chicago merely
because some wire primitives match.

SIGFM is not a project fallback. Introduce a general matcher-backend interface
only when a second native backend or a second officially confirmed Chicago
profile exists. That interface must keep calibration, raw geometry,
preprocessing state, enrollment, templates, identify, adaptive learning, and
device binding as one coherent contract.

## Deliberately not abstracted

The following are not common merely because another driver has similar names:

- USB versus SPI transfer, interrupt and cancellation behavior;
- TLS server/socket integration versus the GDIX51C0 custom SPI BIO;
- reset, register, OTP, PSK, FDT and DAC scheduling and policy;
- frame dimensions, geometry transforms and capture timing;
- calibration and ImageBase formats;
- matcher, enrollment policy, templates and persistent state.

The raw12 byte packing is now common; geometry-dependent regrouping, cropping,
preprocessing and capture timing remain deliberately device-specific.

## Extraction rule

Start a new device as an explicit profile or isolated driver. Compare traces,
fixtures and failure behavior. Extract the smallest shared unit only after both
implementations pass the same deterministic tests. Runtime identifiers and
state paths remain device-specific so refactors cannot invalidate enrollment.
