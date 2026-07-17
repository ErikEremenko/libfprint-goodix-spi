#!/usr/bin/env python3
"""Disassemble a window of code around a given VA in a PE.

Usage:
    04_disasm_window.py <dll> <hex_va> [before_bytes=0x200] [after_bytes=0x200]
"""
import sys, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def main():
    dll = sys.argv[1]
    va = int(sys.argv[2], 16)
    before = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x200
    after  = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0x200

    pe = pefile.PE(dll, fast_load=True)
    img = pe.OPTIONAL_HEADER.ImageBase
    # find which section contains va
    for sec in pe.sections:
        sec_va = img + sec.VirtualAddress
        if sec_va <= va < sec_va + sec.Misc_VirtualSize:
            off = va - sec_va
            data = sec.get_data()
            start = max(0, off - before)
            end = min(len(data), off + after)
            md = Cs(CS_ARCH_X86, CS_MODE_64)
            md.detail = True
            print(f"# section={sec.Name.rstrip(chr(0).encode()).decode()} VA=0x{va:x}  window=0x{sec_va+start:x}-0x{sec_va+end:x}")
            for insn in md.disasm(data[start:end], sec_va + start):
                mark = " <<<" if insn.address == va else ""
                print(f"  0x{insn.address:x}: {insn.mnemonic:<6} {insn.op_str}{mark}")
            return
    print(f"VA 0x{va:x} not in any section")


if __name__ == "__main__":
    main()
