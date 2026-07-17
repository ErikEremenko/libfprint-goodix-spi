// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Quality-gap test: measure AlgoMilan quality/coverage under 4 preprocessing
 * setups on the same frame, to see what closes the gap to Windows (65-94).
 *   qtest.exe <base.raw> <frame.raw>
 * env: INVERT=1  -> feed (4095-raw); REALCAL=1 -> load real_b1/real_b2 factory cal
 * Build: x86_64-w64-mingw32-gcc -O2 qtest.c -o qtest.exe */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#define W 64
#define H 80
#define FB (2*W*H)
#define IB (W*H)
#define SS 0x40
#define P(s,o,v)   (*(void**)((char*)(s)+(o))=(void*)(v))
#define U16(s,o,v) (*(unsigned short*)((char*)(s)+(o))=(unsigned short)(v))
#define U32(s,o,v) (*(unsigned*)((char*)(s)+(o))=(unsigned)(v))
typedef int (*ppp_t)(int); typedef int (*ic_t)(void);
typedef int (*sc_t)(void*,unsigned*,void*,unsigned*); typedef int (*ld_t)(void*,unsigned,void*,unsigned);
typedef int (*sm_t)(int); typedef int (*ppi_t)(void*);
typedef int (*pp_t)(void*,void*,void*,void*,void*,char,char);
#define G(t,n) (t)(void*)GetProcAddress(h,n)
static HMODULE h;
static unsigned short* rd(const char*p){ unsigned short*b=calloc(1,FB);
    FILE*f=fopen(p,"rb"); if(f){ if(fread(b,1,FB,f)){} fclose(f);} return b; }
int main(int argc,char**argv){
    if(argc<3){printf("usage: qtest <base> <frame>\n");return 2;}
    h=LoadLibraryA("AlgoMilan.dll");
    int invert=getenv("INVERT")!=0, realcal=getenv("REALCAL")!=0;
    ((ppp_t)G(ppp_t,"ppp_param_init"))(10);
    ((ic_t)G(ic_t,"preprocess_init_calidata"))();
    ld_t load=G(ld_t,"preprocess_load_calidata");
    if(realcal){
        FILE*f1=fopen("real_b1.bin","rb"),*f2=fopen("real_b2.bin","rb");
        if(!f1||!f2){printf("no real_b1/b2\n");return 1;}
        void*b1=calloc(1,0x184ac); void*b2=calloc(1,0xa004);
        if(fread(b1,1,0x184ac,f1)){} if(fread(b2,1,0xa004,f2)){} fclose(f1);fclose(f2);
        int lr=load(b1,0x184ac,b2,0xa004);
        printf("REALCAL load ret=0x%x\n",lr);
    } else {
        void*c1=calloc(1,0x184ac);void*c2=calloc(1,0xa004);unsigned s1=0x184ac,s2=0xa004;
        ((sc_t)G(sc_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2); load(c1,s1,c2,s2);
    }
    unsigned short* base=rd(argv[1]);
    if(invert) for(int i=0;i<W*H;i++) base[i]=4095-base[i];
    char* cfg=calloc(1,0x40); P(cfg,0x18,base); U32(cfg,0x24,W); U32(cfg,0x28,H);
    ((ppi_t)G(ppi_t,"preprocessor_init"))(cfg);
    ((sm_t)G(sm_t,"preprocess_set_mode"))(1);
    pp_t preprocessor=G(pp_t,"preprocessor");
    unsigned short* raw=rd(argv[2]);
    if(invert) for(int i=0;i<W*H;i++) raw[i]=4095-raw[i];
    unsigned char* enh=calloc(1,IB);
    char* in=calloc(1,SS); char* out=calloc(1,SS); char* qc=calloc(1,0x40);
    void* a1=calloc(1,0x8000); void* a2=calloc(1,0x8000);
    P(in,0,raw);U16(in,8,W);U16(in,0xa,H);U32(in,0x14,FB);U16(in,0x18,1);
    P(out,0,enh);U16(out,8,W);U16(out,0xa,H);U32(out,0x14,IB);
    preprocessor(in,a1,a2,out,qc,0,0);
    { const char*eo=getenv("ENHOUT"); if(eo){ FILE*fo=fopen(eo,"wb"); if(fo){fwrite(enh,1,IB,fo);fclose(fo);} } }
    printf("INVERT=%d REALCAL=%d -> quality=%u coverage=%u\n",
        invert, realcal, (unsigned char)out[0x28], (unsigned char)out[0x29]);
    return 0;
}
