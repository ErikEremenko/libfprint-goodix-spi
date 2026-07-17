#!/usr/bin/env python3
"""Find the *host* PSK identity/ciphersuite strings by xrefing from .text only.

The user's DLL has many duplicate copies of 'Client_identi' and
'TLS-PSK-WITH-AES-128-GCM-SHA256' because each embedded MCU firmware carries
its own mbedtls blob. But only the *host* copies are referenced from .text
(the host code). We enumerate all occurrences, then for each candidate
string we scan .text for a RIP-relative LEA that lands on it.
"""
import sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def scan_literals(data: bytes, needle: bytes):
    """Return offsets in `data` where needle (null-terminated if given so) occurs."""
    out = []
    i = 0
    while True:
        i = data.find(needle, i)
        if i < 0:
            break
        out.append(i)
        i += 1
    return out


def get_text(pe):
    t = next(s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.text')
    return t, t.get_data(), pe.OPTIONAL_HEADER.ImageBase + t.VirtualAddress


def xrefs_from_text(pe, target_vas: set):
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    _, data, base_va = get_text(pe)
    hits = []
    for insn in md.disasm(data, base_va):
        if 'rip' not in insn.op_str:
            continue
        for op in insn.operands:
            if op.type == 3:
                regnm = insn.reg_name(op.mem.base) or ''
                if regnm == 'rip':
                    tgt = insn.address + insn.size + op.mem.disp
                    if tgt in target_vas:
                        hits.append((insn.address, insn.mnemonic, insn.op_str, tgt))
    return hits


def main():
    dll = sys.argv[1]
    print(f"# {dll}")
    pe = pefile.PE(dll, fast_load=True)
    img = pe.OPTIONAL_HEADER.ImageBase
    # Get .rdata data once
    rdata = next(s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.rdata')
    rdata_data = rdata.get_data()
    rdata_va = img + rdata.VirtualAddress

    for label, needle in [
        ("Client_identi\\0\\0",  b"Client_identi\x00\x00"),
        ("Client_identi",         b"Client_identi\x00"),
        ("TLS-PSK-WITH-AES-128-GCM-SHA256", b"TLS-PSK-WITH-AES-128-GCM-SHA256\x00"),
        ("mbedtls_ssl_psk_derive_premaster", b"mbedtls_ssl_psk_derive_premaster\x00"),
    ]:
        offs = scan_literals(rdata_data, needle)
        print(f"\n## {label}: {len(offs)} copies in .rdata")
        va_set = {rdata_va + o for o in offs}
        xrefs = xrefs_from_text(pe, va_set)
        print(f"   xrefs from .text: {len(xrefs)}")
        for ea, mn, ops, tgt in xrefs:
            print(f"     0x{ea:x}: {mn:<4} {ops:<40s}  -> literal @ 0x{tgt:x}")


if __name__ == "__main__":
    main()
