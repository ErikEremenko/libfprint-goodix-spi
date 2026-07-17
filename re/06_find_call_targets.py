#!/usr/bin/env python3
"""Find call sites whose target (direct call) is one of the given VAs.

Usage:
    06_find_call_targets.py <dll> <hex_va> [<hex_va> ...]
"""
import sys, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def main():
    dll = sys.argv[1]
    tgt_vas = {int(x, 16) for x in sys.argv[2:]}
    pe = pefile.PE(dll, fast_load=True)
    img = pe.OPTIONAL_HEADER.ImageBase

    text = next(s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.text')
    data = text.get_data()
    base = img + text.VirtualAddress
    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
    for insn in md.disasm(data, base):
        if insn.mnemonic not in ('call', 'jmp'):
            continue
        for op in insn.operands:
            if op.type == 2:  # IMM
                if op.imm in tgt_vas:
                    print(f"  0x{insn.address:x}: {insn.mnemonic:<4} 0x{op.imm:x}")


if __name__ == "__main__":
    main()
