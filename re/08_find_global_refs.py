#!/usr/bin/env python3
"""Find every .text instruction whose RIP-relative displacement reaches one of the given VAs."""
import sys, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def main():
    dll = sys.argv[1]
    tgts = {int(x, 16) for x in sys.argv[2:]}
    pe = pefile.PE(dll, fast_load=True)
    img = pe.OPTIONAL_HEADER.ImageBase
    text = next(s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.text')
    data = text.get_data()
    base = img + text.VirtualAddress
    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
    for insn in md.disasm(data, base):
        if 'rip' not in insn.op_str:
            continue
        for op in insn.operands:
            if op.type == 3 and (insn.reg_name(op.mem.base) or '') == 'rip':
                t = insn.address + insn.size + op.mem.disp
                if t in tgts:
                    print(f"  0x{insn.address:x}: {insn.mnemonic:<4} {insn.op_str}  -> 0x{t:x}")


if __name__ == "__main__":
    main()
