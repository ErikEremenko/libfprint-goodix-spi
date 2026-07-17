#!/usr/bin/env python3
"""Pure-Python reimplementation of gfspi.dll `whitebox_wrap` @0x180006bb0.

No DLL, no emulator: the "white-box" turned out to be standard AES-256 with a
key derived (once) into a 48-byte buffer. We recovered that buffer via emulation
(re/wb_oracle.py) and the whole container is reproducible with stock primitives:

  KEY      = KBUF[0:32]                       (AES-256-GCM key)
  HMAC_KEY = KBUF[16:48]                      (front HMAC-SHA256 key)
  AAD      = 522dc1f0 99567d07 f47f37a3 2a84427d   (constant GCM AAD)

WB (102 bytes) = front(32) ‖ hdr(2) ‖ len(4) ‖ h16(16) ‖ ct(32) ‖ tag(16):

  hdr  = 02 ff
  len  = uint32_le(len(psk))                  (= 0x20)
  h16  = SHA256(hdr‖len‖psk[0:8]‖(03000000)*16)[0:16]
  ct,tag = AES-256-GCM(KEY, iv=h16, aad=AAD, plaintext=psk)   # 16-byte IV
  front = HMAC-SHA256(HMAC_KEY, hdr‖len‖ct‖tag)

All three key constants are baked into gfspi.dll (same for 1.1.124.12 and
1.2.300.81 per the identical pipeline); verified byte-exact against the DLL
oracle for arbitrary PSKs. See re/PARITY.md.
"""
import hashlib
import hmac
import struct
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

KBUF = bytes.fromhex(
    "58f013eeb7e216e2c1cd0e8fffa7dbd6799cd92aa149013ea0e9f9fd7dc6d94e"
    "3ef63a7598c2a4933129e871ea03043b")
KEY = KBUF[0:32]
HMAC_KEY = KBUF[16:48]
AAD = bytes.fromhex("522dc1f099567d07f47f37a32a84427d")


def wrap_psk(psk: bytes) -> bytes:
    if len(psk) != 32:
        raise ValueError("psk must be 32 bytes")
    hdr = b"\x02\xff"
    ln = struct.pack("<I", len(psk))
    h16 = hashlib.sha256(hdr + ln + psk[0:8] + bytes.fromhex("03000000") * 16).digest()[:16]
    enc = Cipher(algorithms.AES(KEY), modes.GCM(h16)).encryptor()
    enc.authenticate_additional_data(AAD)
    ct = enc.update(psk) + enc.finalize()
    tag = enc.tag
    front = hmac.new(HMAC_KEY, hdr + ln + ct + tag, hashlib.sha256).digest()
    return front + hdr + ln + h16 + ct + tag


def unwrap_wb(wb: bytes) -> bytes:
    """Inverse of wrap_psk: recover the 32-byte PSK from a 102-byte WB.

    Fully authenticates the container (front HMAC, GCM tag, h16 binding) and
    raises ValueError if any check fails — i.e. the blob is not a WB for our
    key.  This is how a dump of MCU register 0xbb010003 becomes the plaintext
    PSK, with no DPAPI.
    """
    if len(wb) != 102:
        raise ValueError(f"WB must be 102 bytes, got {len(wb)}")
    front, hdr, ln = wb[0:0x20], wb[0x20:0x22], wb[0x22:0x26]
    h16, ct, tag = wb[0x26:0x36], wb[0x36:0x56], wb[0x56:0x66]
    if hdr != b"\x02\xff" or ln[0] != 32:
        raise ValueError("not a PSK WB (bad header/length)")
    if not hmac.compare_digest(front, hmac.new(HMAC_KEY, hdr + ln + ct + tag,
                                                hashlib.sha256).digest()):
        raise ValueError("front HMAC mismatch (wrong key or corrupt WB)")
    dec = Cipher(algorithms.AES(KEY), modes.GCM(h16, tag)).decryptor()
    dec.authenticate_additional_data(AAD)
    psk = dec.update(ct) + dec.finalize()   # raises InvalidTag if tampered
    want = hashlib.sha256(hdr + ln + psk[0:8] + bytes.fromhex("03000000") * 16).digest()[:16]
    if not hmac.compare_digest(want, h16):
        raise ValueError("h16 mismatch")
    return psk


if __name__ == "__main__":
    import argparse
    import os
    import sys
    ap = argparse.ArgumentParser(description="pure-python whitebox_wrap / unwrap")
    ap.add_argument("--psk")
    ap.add_argument("--random", action="store_true")
    ap.add_argument("--unwrap", metavar="WB",
                    help="recover PSK from a 102-byte WB (hex, or a file path)")
    args = ap.parse_args()

    if args.unwrap:
        raw = args.unwrap
        try:
            wb = bytes.fromhex("".join(raw.split()))
        except ValueError:
            wb = open(raw, "rb").read()
        try:
            print("psk:", unwrap_wb(wb).hex())
        except Exception as e:
            print(f"unwrap failed: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        psk = os.urandom(32) if args.random else (
            bytes.fromhex(args.psk) if args.psk else
            bytes(range(32)))
        print("psk:", psk.hex())
        print("wb :", wrap_psk(psk).hex())
