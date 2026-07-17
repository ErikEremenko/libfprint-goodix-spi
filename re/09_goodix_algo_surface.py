#!/usr/bin/env python3
"""Map Goodix algorithm/engine DLL surface without Ghidra or capstone.

Usage:
    09_goodix_algo_surface.py <dll> [<dll> ...]

Prints exports, matching-related strings, and coarse RIP-relative xrefs to
those strings.  The xref finder recognizes common x86-64 LEA/MOV/CMP style
RIP-relative encodings and is intentionally conservative.
"""

import os
import re
import struct
import sys

import pefile


KEYWORDS = re.compile(
    rb"(match|auth|verify|ident|enroll|template|quality|score|minutia|feature|"
    rb"extract|compare|threshold|fake|study|cache|image|finger|handle|algo)",
    re.IGNORECASE,
)


def c_strings(data, min_len=5):
    out = []
    for m in re.finditer(rb"[\x20-\x7e]{%d,}" % min_len, data):
        out.append((m.start(), m.group(0).decode("latin1", "replace")))
    return out


def u16_strings(data, min_chars=5):
    out = []
    # ASCII-range UTF-16LE string with trailing NUL not required.
    pat = rb"(?:[\x20-\x7e]\x00){%d,}" % min_chars
    for m in re.finditer(pat, data):
        raw = m.group(0)
        try:
            out.append((m.start(), raw.decode("utf-16le", "replace")))
        except UnicodeDecodeError:
            pass
    return out


def section_name(sec):
    return sec.Name.rstrip(b"\x00").decode("latin1", "replace")


def va_to_file_offset(pe, va):
    rva = va - pe.OPTIONAL_HEADER.ImageBase
    return pe.get_offset_from_rva(rva)


def find_keyword_strings(pe):
    hits = []
    image = pe.OPTIONAL_HEADER.ImageBase
    for sec in pe.sections:
        data = sec.get_data()
        base_va = image + sec.VirtualAddress
        name = section_name(sec)
        for off, s in c_strings(data):
            if KEYWORDS.search(s.encode("latin1", "replace")):
                hits.append((base_va + off, name, "ascii", s))
        for off, s in u16_strings(data):
            if KEYWORDS.search(s.encode("latin1", "replace")):
                hits.append((base_va + off, name, "utf16", s))
    return hits


def rip_xrefs(pe, targets):
    text = next((s for s in pe.sections if section_name(s) == ".text"), None)
    if text is None:
        return []

    data = text.get_data()
    base = pe.OPTIONAL_HEADER.ImageBase + text.VirtualAddress
    targets = set(targets)
    refs = []

    # Common x86-64 RIP-relative forms:
    #   48/4c 8d/8b/89/39/3b xx disp32
    #   8b/89/39/3b/8d xx disp32
    for i in range(0, max(0, len(data) - 7)):
        candidates = []
        b0 = data[i]
        b1 = data[i + 1]
        b2 = data[i + 2]

        if b0 in (0x48, 0x4c) and b1 in (0x8d, 0x8b, 0x89, 0x39, 0x3b, 0x85):
            # ModRM mod=00 r/m=101 means RIP+disp32.
            if (b2 & 0xc7) == 0x05:
                disp = struct.unpack_from("<i", data, i + 3)[0]
                candidates.append((7, disp, f"{b0:02x} {b1:02x} {b2:02x}"))
        elif b0 in (0x8d, 0x8b, 0x89, 0x39, 0x3b, 0x85):
            b1 = data[i + 1]
            if (b1 & 0xc7) == 0x05:
                disp = struct.unpack_from("<i", data, i + 2)[0]
                candidates.append((6, disp, f"{b0:02x} {b1:02x}"))

        for insn_len, disp, prefix in candidates:
            target = base + i + insn_len + disp
            if target in targets:
                refs.append((base + i, target, prefix))
    return refs


def dump_exports(pe):
    if not hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        print("Exports: none")
        return

    exports = pe.DIRECTORY_ENTRY_EXPORT.symbols
    print(f"Exports: {len(exports)}")
    for sym in exports:
        name = (sym.name or b"").decode("latin1", "replace")
        va = pe.OPTIONAL_HEADER.ImageBase + sym.address
        mark = " *" if KEYWORDS.search(name.encode("latin1", "replace")) else ""
        print(f"  0x{va:x} ord={sym.ordinal:<3} {name}{mark}")


def dump_imports(pe):
    if not hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
      return

    interesting = []
    for entry in pe.DIRECTORY_ENTRY_IMPORT:
        dll = entry.dll.decode("latin1", "replace")
        for imp in entry.imports:
            name = (imp.name or b"").decode("latin1", "replace")
            if KEYWORDS.search((dll + "!" + name).encode("latin1", "replace")):
                interesting.append(f"{dll}!{name}")

    if interesting:
        print("Interesting imports:")
        for item in interesting:
            print(f"  {item}")


def show(path):
    pe = pefile.PE(path)
    pe.parse_data_directories()

    print(f"\n=== {path} ({os.path.getsize(path)} bytes) ===")
    print(f"ImageBase=0x{pe.OPTIONAL_HEADER.ImageBase:x} Entry=0x{pe.OPTIONAL_HEADER.AddressOfEntryPoint:x}")
    dump_exports(pe)
    dump_imports(pe)

    hits = find_keyword_strings(pe)
    print(f"Keyword strings: {len(hits)}")
    for va, sec, enc, s in hits[:220]:
        print(f"  0x{va:x} {sec:<8} {enc:<5} {s}")
    if len(hits) > 220:
        print(f"  ... {len(hits) - 220} more")

    refs = rip_xrefs(pe, [va for va, _, _, _ in hits])
    print(f"Keyword xrefs: {len(refs)}")
    names = {va: s for va, _, _, s in hits}
    for ea, target, prefix in refs[:220]:
        print(f"  0x{ea:x} -> 0x{target:x} [{prefix}] {names.get(target, '')}")
    if len(refs) > 220:
        print(f"  ... {len(refs) - 220} more")


def main():
    if len(sys.argv) < 2:
        print(__doc__.strip())
        return 2
    for path in sys.argv[1:]:
        show(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
