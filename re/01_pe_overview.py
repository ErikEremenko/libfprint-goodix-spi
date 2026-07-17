#!/usr/bin/env python3
"""Dump basic PE overview: sections, exports, imports, entropy."""
import sys, os
import pefile

def show(path):
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT'],
        pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT'],
    ])
    print(f"\n=== {path} ({os.path.getsize(path)} bytes) ===")
    print(f"ImageBase: 0x{pe.OPTIONAL_HEADER.ImageBase:x}  "
          f"EP: 0x{pe.OPTIONAL_HEADER.AddressOfEntryPoint:x}  "
          f"Machine: 0x{pe.FILE_HEADER.Machine:x}")
    print("Sections:")
    for s in pe.sections:
        name = s.Name.rstrip(b'\x00').decode('latin1')
        print(f"  {name:<10}  VA=0x{s.VirtualAddress:08x}  VSz=0x{s.Misc_VirtualSize:08x}  "
              f"RAW=0x{s.PointerToRawData:08x}/0x{s.SizeOfRawData:08x}  "
              f"entropy={s.get_entropy():.2f}")
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        n = sum(len(e.imports) for e in pe.DIRECTORY_ENTRY_IMPORT)
        print(f"Imports: {n} from {len(pe.DIRECTORY_ENTRY_IMPORT)} DLLs")
        for e in pe.DIRECTORY_ENTRY_IMPORT[:50]:
            print(f"  {e.dll.decode('latin1','replace')}")
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        print(f"Exports: {len(pe.DIRECTORY_ENTRY_EXPORT.symbols)}")
        for s in pe.DIRECTORY_ENTRY_EXPORT.symbols[:20]:
            nm = (s.name or b'').decode('latin1', 'replace')
            print(f"  ord={s.ordinal:<4} RVA=0x{s.address:08x} {nm}")

if __name__ == "__main__":
    for p in sys.argv[1:]:
        show(p)
