# Driver tests

The test sources live in this repository and are copied into the fetched or
explicitly supplied libfprint tree by the shared preparation script. This keeps
the out-of-tree driver and its tests versioned together without keeping a
libfprint checkout inside this repository.

- `goodix-common/` validates the command wire contract, sensor-profile map,
  raw12 stream decoder, and CRC-32/MPEG-2 primitive shared only where other
  Goodix implementations prove identical behavior.
- `goodix-chicago/` validates the GDIX51C0 Chicago calibration, preprocessing,
  feature, enrollment, matcher, BIR, and template layers.

After configuring libfprint, run every test registered in the `goodix-spi`
Meson suite with:

```sh
./tests/run-driver-tests.sh
```

By default the runner resolves the pinned cached checkout through
`scripts/fetch-libfprint.sh`. Pass a build directory as the first argument, or
set `LIBFPRINT_BUILD`, to select another build.

New sensor tests belong in a `tests/goodix-*` directory and must be registered
in `integration/libfprint-tests/meson.build`. Suite membership is assigned by
that fragment, so the runner and CI discover new registered tests without a
second hardcoded list.

Oracle-backed cases use environment variables documented in the corresponding
test source and reverse-engineering runner. Without private captures, those
cases report `SKIP`; deterministic unit cases still run.

## Hardware fault tests

The retry-boundary helper requires a developer build because release libraries
do not contain fault-injection code:

```sh
GOODIX_SPI_DEVELOPER=1 ./install.sh
./scripts/gdix51c0/test-spi-retry.sh response
```

`scripts/ci/check-build-profile.sh` verifies that release binaries contain no
capture-dump or fault-injection markers and that developer binaries contain the
expected hooks.
