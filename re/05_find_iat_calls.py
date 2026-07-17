#!/usr/bin/env python3
"""Find every call site in .text that targets a given imported API name (IAT entry).

Usage:
    05_find_iat_calls.py <dll> <imported-name>  [<imported-name> ...]

Example:
    05_find_iat_calls.py gfspi.dll CryptUnprotectData CryptProtectData
"""
import sys, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def main():
    dll = sys.argv[1]
    apis = set(sys.argv[2:])
    pe = pefile.PE(dll, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
    img = pe.OPTIONAL_HEADER.ImageBase

    # Build: IAT-entry VA -> name
    iat_map = {}
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for d in pe.DIRECTORY_ENTRY_IMPORT:
            for imp in d.imports:
                name = (imp.name or b'').decode('latin1', 'replace')
                if imp.address:
                    iat_map[imp.address] = name

    targets = {va: name for va, name in iat_map.items() if name in apis}
    if not targets:
        print(f"No IAT entry matches any of {apis}")
        print("First 20 IAT entries:")
        for v, n in list(iat_map.items())[:20]:
            print(f"  0x{v:x} -> {n}")
        return
    print(f"# {dll}  iat hits:")
    for v, n in targets.items():
        print(f"  0x{v:x} -> {n}")

    text = next(s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.text')
    data = text.get_data()
    base = img + text.VirtualAddress
    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True

    print("\n# call sites:")
    for insn in md.disasm(data, base):
        if insn.mnemonic not in ('call', 'jmp'):
            continue
        if not insn.operands:
            continue
        # indirect call through [rip+disp] - typical IAT call: ff 15 xx xx xx xx
        for op in insn.operands:
            if op.type == 3 and (insn.reg_name(op.mem.base) or '') == 'rip':
                tgt = insn.address + insn.size + op.mem.disp
                if tgt in targets:
                    print(f"  0x{insn.address:x}: {insn.mnemonic:<4} {insn.op_str}  -> {targets[tgt]}")


if __name__ == "__main__":
    main()
