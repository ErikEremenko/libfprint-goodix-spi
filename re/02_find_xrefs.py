#!/usr/bin/env python3
"""Find xrefs to given ASCII (or UTF-16LE) strings in a PE.

Usage:
    02_find_xrefs.py <dll> <string> [<string> ...]

For each match, scans .text for RIP-relative LEAs/MOVs that reach the
address of the matched string, reporting (section_VA, rel_offset, instr).
"""
import sys, os
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def find_string_vas(pe: pefile.PE, needle_bytes: bytes):
    """Return list of VAs (ImageBase + RVA) where needle appears in any section."""
    hits = []
    img_base = pe.OPTIONAL_HEADER.ImageBase
    for sec in pe.sections:
        data = sec.get_data()
        start = 0
        while True:
            idx = data.find(needle_bytes, start)
            if idx == -1:
                break
            va = img_base + sec.VirtualAddress + idx
            hits.append((sec, va, idx))
            start = idx + 1
    return hits


def disasm_xrefs_to_va(pe: pefile.PE, target_vas):
    """Disassemble .text linearly, report instructions whose RIP-relative displacement
    points to any target VA (only LEA / MOV with rip displacement)."""
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    img_base = pe.OPTIONAL_HEADER.ImageBase
    # Identify .text
    text = next(s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.text')
    data = text.get_data()
    text_va = img_base + text.VirtualAddress
    targets = set(target_vas)
    results = []
    for insn in md.disasm(data, text_va):
        if not insn.operands:
            continue
        # Looking for any operand referencing [rip + disp] where rip = insn.address + insn.size
        for op in insn.operands:
            # op.type == 3 means MEM for x86 in capstone
            if op.type == 3 and op.mem.base == 0x13:  # X86_REG_RIP = 41 (0x29) in capstone 5
                pass
            # Easier: use insn.op_str parsing and manual calc.
        # fallback via op_str
        if 'rip' in insn.op_str:
            # Parse disp out of operand structure robustly
            for op in insn.operands:
                if op.type == 3 and op.mem.base != 0:  # memory operand
                    reg_name = insn.reg_name(op.mem.base) or ''
                    if reg_name == 'rip':
                        disp = op.mem.disp
                        tgt = insn.address + insn.size + disp
                        if tgt in targets:
                            results.append((insn.address, insn.mnemonic, insn.op_str, tgt))
    return results


def main():
    dll = sys.argv[1]
    needles = sys.argv[2:]
    pe = pefile.PE(dll)
    print(f"# {dll}  imagebase=0x{pe.OPTIONAL_HEADER.ImageBase:x}")
    for n in needles:
        n_bytes_ascii = n.encode('utf-8') + b'\x00'
        n_bytes_wide  = n.encode('utf-16le') + b'\x00\x00'
        print(f"\n## needle: {n!r}")
        for label, nb in (("ascii", n_bytes_ascii), ("utf16le", n_bytes_wide)):
            hits = find_string_vas(pe, nb)
            for sec, va, _ in hits:
                sec_name = sec.Name.rstrip(b'\x00').decode('latin1')
                print(f"   hit({label}) at 0x{va:x} (sec={sec_name})")
        # xrefs to ascii only, which is what the compiler actually references via LEA
        ascii_vas = [va for _, va, _ in find_string_vas(pe, n_bytes_ascii)]
        if not ascii_vas:
            continue
        xrefs = disasm_xrefs_to_va(pe, ascii_vas)
        for ea, mn, ops, tgt in xrefs:
            print(f"   xref: 0x{ea:x}: {mn} {ops}   -> 0x{tgt:x}")


if __name__ == "__main__":
    main()
