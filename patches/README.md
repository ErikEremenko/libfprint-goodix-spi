# Upstream integration patches

`libfprint/1.94.10.patch` applies to the fetched upstream `v1.94.10` tag at
`0c97a47d8ef405cd577b87058c1e89cae9d242e7`. It contains only generic Meson
hooks. The driver lists, source lists, helper dependencies, and tests live in
the source-owned files under `integration/`, so adding a supported driver does
not require growing the versioned upstream patch.

`fprintd/1.94.5-goodix-spi.patch` applies to fprintd `v1.94.5`. It persists a
print only when a successful matcher mutates its serialized data and adds an
opt-in warm-device lifecycle. Warm devices are opened during enumeration,
retained across Claim/Release, closed before suspend, and reopened after
resume; capture authorization remains unchanged.

When either pinned upstream revision changes, rebase its component patch
directly onto the new clean fetched tree and replace it. Do not add migration
patches between old injected layouts: those layouts are not released
interfaces.
