import sys, struct, pefile
pe = pefile.PE(sys.argv[1], fast_load=True)
img = pe.OPTIONAL_HEADER.ImageBase
targets = [int(a,16) for a in sys.argv[2:]]
secs = [(s.PointerToRawData, s.SizeOfRawData, img+s.VirtualAddress) for s in pe.sections]
data = pe.__data__
def va_at(fo):
    for p,sz,va in secs:
        if p<=fo<p+sz: return va+(fo-p)
    return None
# lea reg,[rip+disp32]: [REX 48/4C] 8D modrm(mod=00,rm=101) disp32  (len 7 with REX, 6 without)
for i in range(len(data)-7):
    for rex,ln in ((True,7),(False,6)):
        j=i
        if rex:
            if data[i] not in (0x48,0x4c): continue
            j=i+1
        if data[j]!=0x8D: continue
        modrm=data[j+1]
        if (modrm & 0xC7)!=0x05: continue
        va=va_at(i)
        if va is None: continue
        disp=struct.unpack_from("<i",data,j+2)[0]
        tgt=va+ln+disp
        if tgt in targets:
            print(f"0x{va:x}: lea reg{(modrm>>3)&7},[0x{tgt:x}]")
