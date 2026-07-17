# Source provenance

This project combines independently written Linux code, adaptations of free
software, and interoperability information learned from hardware and Windows
driver behavior. This file records those boundaries so that attribution does
not depend on Git history and survives rebases or squashed releases.

Unless a file or section says otherwise, original source in this repository is
Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> and is distributed under
LGPL-2.1-or-later. Existing third-party notices remain authoritative.

## Adapted free-software source

### GoodixTLS libfprint branch

- Repository: <https://github.com/goodix-fp-linux-dev/libfprint>
- Revision reviewed: `c343b6934e40dcd40a5f9e3095810d98f1175a4d`
  (`goodixtls` branch)
- License: LGPL-2.1-or-later, as stated in the source headers and `COPYING`
- Copyright holders retained from relevant files:
  Alexander Meiler, Matthieu CHARETTE, and Natasha England-Elbro
- Use here: Goodix command identifiers and payload layouts, wire framing, and
  packed raw12 decoding were adapted into transport-neutral helpers under
  `lib/goodix/common/`. The SPI transport, session lifecycle, and native
  Chicago backend are not copies of the GoodixTLS USB driver.

### Goodix 53x5 libfprint driver

- Repository: <https://github.com/seaweeduk/goodix53x5-libfprint>
- Revision reviewed: `662f40be66ff46febbe358a601f4b29faf46ef35`
- License: LGPL-2.1-or-later, as stated in the individual source headers
- Copyright holder retained from relevant files:
  `goodix-fp-linux-dev contributors`
- Use here: the common wire/raw12 primitives, CRC-32/MPEG-2 behavior, and MCU
  configuration checksum were compared with or adapted from the 53x5
  implementation. Sensor-specific 53x5 transport, calibration, and SIGFM code
  are not included.

## Interoperability and behavioral references

The repositories in this section supplied protocol facts, traces, analysis
material, or test expectations. Their source code and prose are not vendored.

### Goodix FP Dump

- Repository: <https://github.com/goodix-fp-linux-dev/goodix-fp-dump>
- Revision reviewed: `cc43bb3b3154a0bccc0412ae024013c7e1923139`
- Repository license: MIT
- Use here: Goodix command behavior, device-family comparisons, packet
  framing, image packing, and initialization experiments.

### GDIX51C0 SPI reversing

- Repository: <https://github.com/lexakimov/goodix51c0_spi-reversing>
- Revision reviewed: `63277ed23e18ae8ded8affe66330a968f503db5a`
- License status: no license file or source license notice was present at that
  revision
- Use here: GDIX51C0 packet formats, SPI/GPIO interaction, command ordering,
  image packing, and captured protocol behavior were treated as
  interoperability facts. The Python implementation and documentation are not
  vendored or copied verbatim; the production implementation was written in C
  and validated independently against hardware and driver traces.

### Goodix Ghidra projects

- Repository: <https://github.com/goodix-fp-linux-dev/goodix-ghidra>
- Revision reviewed: `36b5be054aff5c00e60d12c426056f77f8b76eb0`
- Repository license file: MIT
- Use here: decompiled-driver analysis and behavioral investigation only.
  The Ghidra projects contain vendor-derived material, so this project does not
  rely on the repository's MIT file as permission to copy or redistribute
  Goodix program expression. No `.gzf`, DLL, SYS, or firmware file is tracked
  here.

### Goodix firmware archive

- Repository: <https://github.com/goodix-fp-linux-dev/goodix-firmware>
- Revision reviewed: `7b9a828d1d14dee587d9c900505233188c23a92d`
- Repository license file: MIT; its README separately identifies the firmware
  binaries as Goodix property and asks that they remain hosted in that archive
- Use here: firmware identity and analysis reference only. No firmware binary
  is copied or redistributed by this repository.

### Official Windows driver stack

The GDIX51C0 transport, white-box PSK container, calibration, and ChicagoHS
matcher were investigated using installed Windows driver binaries,
hardware traces, Wine oracle programs, and differential tests. The Linux
source is a new implementation of the observed interfaces and behavior. The
vendor DLLs, SYS files, firmware, disassembly databases, fingerprint captures,
templates, calibration state, and device credentials are intentionally not
distributed. See `re/README.md` and `re/PARITY.md` for the public research
boundary.

## Target-project patches

- `patches/libfprint/` targets LGPL-2.1-or-later libfprint and is distributed
  under that target project's terms.
- `patches/fprintd/` modifies GPL-2.0-or-later fprintd source and is distributed
  under that target project's terms, not under the repository-wide LGPL
  default.

## Attribution policy

- Files copied or adapted from free software retain the original copyright and
  license notices, followed by Berke Kabagöz's notice for the new work.
- Files written from protocol facts, observed behavior, or test results carry
  the new implementation's notice; the behavioral source is recorded here.
- A repository reference is not treated as permission to copy material that is
  unlicensed or owned by a third party.
- New source files should include an SPDX identifier matching their actual
  license before a public release.
