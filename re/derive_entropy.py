#!/usr/bin/env python3
"""Linux reimplementation of gfspi.dll `derive_entropy` @0x1800423a4.

This is the function that turns the 8-byte seed (stored on the MCU alongside
the DPAPI blob, i.e. the last 8 bytes of `read 0xbb010002` /
`Goodix_Cache.seed.bin`) into the 48-byte `optionalEntropy` argument passed to
`CryptUnprotectData`. Reproducing it lets us do the DPAPI unseal offline on
Linux (see re/PARITY.md).

Reconstructed purely from static analysis of the 1.2.300.81 DLL:

    H1 = SHA256(seed)                         # seed is 8 bytes
    H2 = SHA256(H1[0:16] || BUF_A)            # BUF_A is a 16-byte DLL constant
    entropy = H1[16:32] || H2                 # 48 bytes

BUF_A is folded at runtime from three 16-byte constant tables in .data
(0x1804e2df0/0x1804e2e00/0x1804e2e10) by two XOR loops; the fold is constant,
so we bake the result. The exact fold (M = the 48 contiguous .data bytes):
    for i in 0..7 : BUF_A[i]   = M[i]      ^ M[0x10+i] ^ M[0x08+i]
    for i in 8..15: BUF_A[i]   = M[0x10+i] ^ M[0x20+i] ^ M[0x18+i]

NOTE: `sha256_alg6` @0x1800072d0 is the DLL's crypto-dispatch algorithm id=6,
which static analysis indicates is standard SHA-256 (standard H0..H7 IV). This
is unverified against a live sample -- before trusting the unseal, confirm the
whole chain against one known (seed -> entropy -> PSK) triple, e.g. the
`Goodix_Cache.bin` blob + a real Windows masterkey.
"""
import hashlib

# 16-byte constant folded out of the .data tables (see module docstring).
BUF_A = bytes.fromhex("04e0b0f3f5598417dde298e467c795f7")


def derive_entropy(seed: bytes) -> bytes:
    if len(seed) != 8:
        raise ValueError(f"seed must be 8 bytes, got {len(seed)}")
    h1 = hashlib.sha256(seed).digest()
    h2 = hashlib.sha256(h1[0:16] + BUF_A).digest()
    entropy = h1[16:32] + h2
    assert len(entropy) == 48
    return entropy


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        sys.exit("usage: derive_entropy.py <seed-hex-or-path>")
    arg = sys.argv[1]
    try:
        seed = bytes.fromhex(arg)
    except ValueError:
        seed = open(arg, "rb").read()
    print(derive_entropy(seed).hex())
