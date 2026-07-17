# Roadmap

GDIX51C0 is the reference implementation and remains the only supported
device. Repository branding may become more general without changing the
runtime driver id, fprintd storage path, environment variables, state files,
or existing enrollment format.

## Reference milestone

Completed on GDIX51C0 hardware:

- enrollment and verification with the native ChicagoHS backend;
- adaptive learning to 50 subtemplates and capacity replacement;
- fprintd persistence, daemon restart, and post-restart matching;
- automatic first-run calibration and later ImageBase refresh;
- official transport retry boundaries and failed-session recovery;
- stale-PSK repair for dual boot;
- idle and active-action suspend/resume.

Before a public reference release, create a clean squashed revision, run the
complete regression set, and tag it without private RE fixtures.

## Adding a sensor

Start the next sensor as an isolated profile or driver. Collect its identity,
transport/init traces, security handshake, OTP and calibration contract, frame
format, official matcher selection, enrollment behavior, and hardware results.
Do not infer compatibility from the Goodix brand or similar command names.

Once that evidence exists, the repository registration path is deliberately
small:

1. Add the device implementation under `drivers/<driver-id>/`.
2. Add its driver id, ACPI id, and install-hook name to
   `config/supported-spi-devices.tsv`.
3. Add device-specific environment, access, and purge behavior under
   `config/install.d/`.
4. Register its source and private helper dependencies in the source-owned
   `integration/` Meson fragments.
5. Add deterministic contracts and hardware acceptance tests.

The installer then synchronizes the declared sources, verifies registration
consistency and attached hardware, and installs libfprint's generated combined
udev rule. No new hand-written spidev rule, modules-load file, install path, or
versioned patch hunk should be needed.

Code becomes common only after at least two implementations pass the same
deterministic contract tests. Candidate shared boundaries include device
identity, wire codec, raw packing, checksums, and fixed command serialization.
Transport scheduling, GPIO, TLS integration, capture timing, geometry,
calibration, templates, and matcher policy remain device-specific until proven
otherwise.

## Known candidates

- GoodixTLS 5110 proves some wire and raw12 primitives, but its USB transport,
  88x80 capture/crop, security integration, and official matcher selection need
  independent evidence.
- Goodix 53x5 proves the inner command envelope, raw12 unpacking, and
  CRC-32/MPEG-2. Its 108x88 GTLS/GEA path is distinct, and the checked reference
  implementation's use of SIGFM does not establish the official matcher
  selection or GDIX51C0 Chicago compatibility.

SIGFM will not be adopted as the generalized fallback. A new device needs a
native backend matching its official algorithm family, or oracle evidence that
an existing backend accepts its exact geometry and calibration.

## Integration direction

Keep the integration patches small and reproducible while the driver remains
out of tree. If selected components are proposed to libfprint later, separate
the common helpers, Chicago backend, GDIX51C0 transport/device integration,
deterministic tests, and hardware evidence into reviewable commits. Such a
submission remains optional and is not assumed by this roadmap.
