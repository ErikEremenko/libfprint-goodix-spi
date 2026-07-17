// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Sweep the preprocessor base and report AlgoMilan quality/coverage for a fixed
 * probe frame, to find what drives quality.
 *   qsweep.exe <probe.raw> <base1.raw> [base2.raw ...]
 * Build: x86_64-w64-mingw32-gcc -O2 qsweep.c -o qsweep.exe */
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
int main(int argc,char**argv){
    if(argc<3){printf("usage: qsweep <probe> <base...>\n");return 2;}
    h=LoadLibraryA("AlgoMilan.dll");
    unsigned short* probe=calloc(1,FB);
    { FILE*f=fopen(argv[1],"rb"); if(f){if(fread(probe,1,FB,f)){}fclose(f);} }
    ((ppp_t)G(ppp_t,"ppp_param_init"))(10);
    ((ic_t)G(ic_t,"preprocess_init_calidata"))();
    ld_t load=G(ld_t,"preprocess_load_calidata");
    sc_t save=G(sc_t,"preprocess_save_calidata");
    ppi_t ppinit=G(ppi_t,"preprocessor_init");
    sm_t setmode=G(sm_t,"preprocess_set_mode");
    pp_t preprocessor=G(pp_t,"preprocessor");
    for(int b=2;b<argc;b++){
        /* re-load default calidata each iter */
        void*c1=calloc(1,0x184ac);void*c2=calloc(1,0xa004);unsigned s1=0x184ac,s2=0xa004;
        save(c1,&s1,c2,&s2); load(c1,s1,c2,s2);
        unsigned short* basefr=calloc(1,FB);
        { FILE*bf=fopen(argv[b],"rb"); if(bf){if(fread(basefr,1,FB,bf)){}fclose(bf);} }
        char* cfg=calloc(1,0x40); P(cfg,0x18,basefr); U32(cfg,0x24,W); U32(cfg,0x28,H);
        ppinit(cfg); setmode(1);
        unsigned short* raw=calloc(1,FB); memcpy(raw,probe,FB);
        unsigned char* enh=calloc(1,IB);
        char* in=calloc(1,SS); char* out=calloc(1,SS); char* qc=calloc(1,0x20);
        void* a1=calloc(1,0x8000); void* a2=calloc(1,0x8000);
        P(in,0,raw);U16(in,8,W);U16(in,0xa,H);U32(in,0x14,FB);U16(in,0x18,1);
        P(out,0,enh);U16(out,8,W);U16(out,0xa,H);U32(out,0x14,IB);
        preprocessor(in,a1,a2,out,qc,0,0);
        int q=(unsigned char)out[0x28], c=(unsigned char)out[0x29];
        printf("base=%-42s q=%3d cov=%3d\n", argv[b], q, c);
        free(c1);free(c2);free(basefr);free(cfg);free(raw);free(enh);free(in);free(out);free(qc);free(a1);free(a2);
    }
    return 0;
}
