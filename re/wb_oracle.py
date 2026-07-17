#!/usr/bin/env python3
"""Unicorn-based oracle for gfspi.dll `whitebox_wrap` @0x180006bb0.

Purpose: run the white-box PSK-wrapping routine outside Windows to (a) wrap an
arbitrary chosen 32-byte PSK into the 102-byte container the MCU accepts, and
(b) dump the constant 48-byte key buffer (`[rbp-0x30]`) that the white-box
branch derives -- from which the AES-GCM key and the HMAC key are sliced. Once
that buffer is known, the whole wrap is reproducible in pure Python (see
re/wb_wrap.py), so this emulator is only needed once / for validation.

We map the DLL at its preferred ImageBase (0x180000000, so no relocation),
provide a stack + a bump heap, and stub the only non-crypto externals the
routine and its mbedtls callees touch: malloc/free/memset/memcpy and the
stack-cookie check. All SHA-256 / AES-GCM / white-box code runs natively in the
emulator.
"""
import struct
import sys
import pefile
from unicorn import *
from unicorn.x86_const import *

DLL = "gfspi.inf_amd64_9b379ce317eda6c3/gfspi.dll"

IMAGE_BASE = 0x180000000
WB_WRAP    = 0x180006bb0
KEY_DUMP_AT = 0x180006e08   # right after 0x180005b50 returns: [rbp-0x30] is the key buf

# CRT / heap entry points to stub (resolved from earlier static analysis).
MALLOC   = 0x18008ad3c
FREE     = 0x180089e20
MEMSET   = 0x1800841f0
MEMCPY   = 0x180084400
MEMCPY2  = 0x180084a90
SECCHECK = 0x180083590

STACK_BASE = 0x00007fff0000000
STACK_SIZE = 0x200000
HEAP_BASE  = 0x00000000aa000000
HEAP_SIZE  = 0x400000
SENTINEL   = 0x00000000cafe0000   # fake return address; run stops here
IO_BASE    = 0x00000000bb000000    # psk in / out buffers / out_len


def align_down(x, a=0x1000): return x & ~(a - 1)
def align_up(x, a=0x1000):   return (x + a - 1) & ~(a - 1)


class Wrapper:
    def __init__(self, dll):
        self.pe = pefile.PE(dll)
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.heap_ptr = HEAP_BASE + 0x1000
        self.key_buf = None
        self._map_image()
        self._map_regions()
        self._hooks()

    def _map_image(self):
        uc = self.uc
        data = self.pe.get_memory_mapped_image(ImageBase=IMAGE_BASE)
        size = align_up(len(data))
        uc.mem_map(IMAGE_BASE, size, UC_PROT_ALL)
        uc.mem_write(IMAGE_BASE, data)
        self.image_end = IMAGE_BASE + size

    def _map_regions(self):
        uc = self.uc
        uc.mem_map(STACK_BASE, STACK_SIZE, UC_PROT_READ | UC_PROT_WRITE)
        uc.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_READ | UC_PROT_WRITE)
        uc.mem_map(IO_BASE, 0x10000, UC_PROT_READ | UC_PROT_WRITE)
        uc.mem_map(align_down(SENTINEL), 0x1000, UC_PROT_ALL)
        uc.mem_write(SENTINEL, b"\xc3")   # ret, in case it ever executes

    def _ret(self, rax=None):
        uc = self.uc
        rsp = uc.reg_read(UC_X86_REG_RSP)
        ret = struct.unpack("<Q", uc.mem_read(rsp, 8))[0]
        uc.reg_write(UC_X86_REG_RSP, rsp + 8)
        uc.reg_write(UC_X86_REG_RIP, ret)
        if rax is not None:
            uc.reg_write(UC_X86_REG_RAX, rax)

    def _alloc(self, n):
        p = self.heap_ptr
        self.heap_ptr = align_up(self.heap_ptr + n, 0x10)
        return p

    def _hooks(self):
        self.uc.hook_add(UC_HOOK_CODE, self._code)
        self.uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._unmapped)

    def _code(self, uc, addr, size, ud):
        if addr == SENTINEL:
            uc.emu_stop(); return
        if addr == KEY_DUMP_AT:
            rbp = uc.reg_read(UC_X86_REG_RBP)
            self.key_buf = bytes(uc.mem_read(rbp - 0x30, 0x30))
        elif addr == MALLOC:
            n = uc.reg_read(UC_X86_REG_RCX)
            self._ret(self._alloc(n))
        elif addr in (FREE, SECCHECK):
            self._ret(0)
        elif addr == MEMSET:
            dst = uc.reg_read(UC_X86_REG_RCX)
            val = uc.reg_read(UC_X86_REG_RDX) & 0xff
            n   = uc.reg_read(UC_X86_REG_R8)
            uc.mem_write(dst, bytes([val]) * n)
            self._ret(dst)
        elif addr in (MEMCPY, MEMCPY2):
            dst = uc.reg_read(UC_X86_REG_RCX)
            src = uc.reg_read(UC_X86_REG_RDX)
            n   = uc.reg_read(UC_X86_REG_R8)
            uc.mem_write(dst, bytes(uc.mem_read(src, n)))
            self._ret(dst)

    def _unmapped(self, uc, access, addr, size, value, ud):
        rip = uc.reg_read(UC_X86_REG_RIP)
        print(f"[UNMAPPED] access={access} addr=0x{addr:x} size={size} rip=0x{rip:x}")
        return False

    def wrap(self, psk: bytes):
        uc = self.uc
        assert len(psk) == 32
        psk_addr = IO_BASE + 0x100
        out_addr = IO_BASE + 0x200
        len_addr = IO_BASE + 0x400
        uc.mem_write(psk_addr, psk)
        uc.mem_write(out_addr, b"\x00" * 0x200)
        uc.mem_write(len_addr, struct.pack("<I", 0x200))   # capacity

        rsp = STACK_BASE + STACK_SIZE - 0x1000
        rsp &= ~0xf
        rsp -= 8                          # so that after push ret, alignment matches call
        uc.mem_write(rsp, struct.pack("<Q", SENTINEL))
        uc.reg_write(UC_X86_REG_RSP, rsp)
        uc.reg_write(UC_X86_REG_RCX, psk_addr)   # arg0 psk
        uc.reg_write(UC_X86_REG_RDX, len(psk))   # arg1 psk_len
        uc.reg_write(UC_X86_REG_R8,  out_addr)   # arg2 out_buf
        uc.reg_write(UC_X86_REG_R9,  len_addr)   # arg3 out_len_ptr

        uc.emu_start(WB_WRAP, SENTINEL)
        rax = uc.reg_read(UC_X86_REG_RAX)
        out_len = struct.unpack("<I", uc.mem_read(len_addr, 4))[0]
        out = bytes(uc.mem_read(out_addr, out_len if 0 < out_len <= 0x200 else 0x80))
        return rax, out_len, out


# Deliberately synthetic ground-truth pair.  Do not use captured device keys as
# committed test fixtures.
SAMPLE_PSK = bytes(range(32))
SAMPLE_WB = bytes.fromhex(
    "45a3732acf2990f4ceb30e8996e4add12e887160d6dcaa99c8bcd8338c7743f2"
    "02ff20000000e7bc09962aedfb470fac3887de32465c5a71c88fe01dd776cfa5"
    "19e70c63f82401f8247324fde9c6562321d4d82b0dd552c0bf1f74716085b852"
    "658bfe772964")


def wrap_psk(psk: bytes, dll: str = DLL) -> bytes:
    """Return the 102-byte MCU PSK-WB container for a chosen 32-byte PSK."""
    w = Wrapper(dll)
    rax, out_len, out = w.wrap(psk)
    if rax != 0 or out_len != 102:
        raise RuntimeError(f"wrap failed: ret=0x{rax & 0xffffffff:x} len={out_len}")
    return out


if __name__ == "__main__":
    import argparse
    import os
    ap = argparse.ArgumentParser(description="whitebox_wrap oracle (emulated)")
    ap.add_argument("--dll", default=DLL)
    ap.add_argument("--psk", help="32-byte PSK in hex to wrap")
    ap.add_argument("--random", action="store_true", help="wrap a random PSK")
    ap.add_argument("--selftest", action="store_true",
                    help="verify byte-exact against the known sample")
    args = ap.parse_args()

    if args.selftest or not (args.psk or args.random):
        wb = wrap_psk(SAMPLE_PSK, args.dll)
        ok = wb == SAMPLE_WB
        print("selftest byte-exact vs sample:", ok)
        if not ok:
            print("  got:", wb.hex())
            print("  exp:", SAMPLE_WB.hex())
            raise SystemExit(1)

    if args.psk or args.random:
        psk = os.urandom(32) if args.random else bytes.fromhex(args.psk)
        wb = wrap_psk(psk, args.dll)
        print("psk:", psk.hex())
        print("wb :", wb.hex())
