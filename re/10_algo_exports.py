#!/usr/bin/env python3
"""Dump AlgoMilan.dll (and friends) exports as name -> VA, sorted by address.

Gives the virtual addresses to feed 04_disasm_window.py for the matching-algorithm
reverse engineering (see re/PARITY.md and the algorithm oracle tools).
"""
import sys
import pefile


def main():
    dll = sys.argv[1]
    pe = pefile.PE(dll, fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
    base = pe.OPTIONAL_HEADER.ImageBase
    print(f"# {dll}  ImageBase=0x{base:x}")

    rows = []
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if e.name:
                rows.append((base + e.address, e.name.decode()))
    rows.sort()
    # print in address order, with the gap to the next export as a size hint
    for i, (va, name) in enumerate(rows):
        nxt = rows[i + 1][0] if i + 1 < len(rows) else va
        gap = nxt - va
        print(f"0x{va:x}  size~0x{gap:<5x}  {name}")


if __name__ == "__main__":
    main()
