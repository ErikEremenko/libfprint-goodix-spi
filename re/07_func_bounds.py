#!/usr/bin/env python3
"""Find function start / end for a given VA using .pdata RUNTIME_FUNCTION entries.

Usage:
    07_func_bounds.py <dll> <hex_va> [<hex_va> ...]
"""
import sys, struct, pefile


def main():
    dll = sys.argv[1]
    vas = [int(x, 16) for x in sys.argv[2:]]
    pe = pefile.PE(dll, fast_load=True)
    img = pe.OPTIONAL_HEADER.ImageBase
    pdata = next(s for s in pe.sections if s.Name.rstrip(b'\x00') == b'.pdata')
    d = pdata.get_data()
    entries = []
    for i in range(0, len(d) - 11, 12):
        begin, end, unwind = struct.unpack_from('<III', d, i)
        if begin == 0 and end == 0:
            continue
        entries.append((img + begin, img + end))
    entries.sort()
    for va in vas:
        # find entry whose [begin, end) contains va
        lo, hi = 0, len(entries)
        while lo < hi:
            mid = (lo + hi) // 2
            b, e = entries[mid]
            if va < b: hi = mid
            elif va >= e: lo = mid + 1
            else:
                print(f"0x{va:x}  func=[0x{b:x}, 0x{e:x}) size={e-b}")
                break
        else:
            print(f"0x{va:x}  no function entry found")


if __name__ == "__main__":
    main()
