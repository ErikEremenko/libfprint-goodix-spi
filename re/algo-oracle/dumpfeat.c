// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Enroll ONE frame, dump its extracted feature array (f0 x 0x38 bytes) to
 * <out>.  These are Goodix's binary local descriptors — feed to a native matcher.
 *   dumpfeat.exe <nofinger.raw> <frame.raw> <out.feat>
 * Build: x86_64-w64-mingw32-gcc -O2 dumpfeat.c -o dumpfeat.exe */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#define W 64
#define H 80
#define FB (2*W*H)
#define IB (W*H)
#define SS 0x40
#define P(s,o,v) (*(void**)((char*)(s)+(o))=(void*)(v))
#define U16(s,o,v) (*(unsigned short*)((char*)(s)+(o))=(unsigned short)(v))
#define U32(s,o,v) (*(unsigned*)((char*)(s)+(o))=(unsigned)(v))
#define U8(s,o,v)  (*(unsigned char*)((char*)(s)+(o))=(unsigned char)(v))
typedef int(*ppp_t)(int);typedef int(*ic_t)(void);
typedef int(*sc_t)(void*,unsigned*,void*,unsigned*);typedef int(*ld_t)(void*,unsigned,void*,unsigned);
typedef int(*sm_t)(int);typedef int(*ppi_t)(void*);
typedef int(*pp_t)(void*,void*,void*,void*,void*,char,char);
typedef void*(*es_t)(void);typedef int(*eai_t)(void*,void*,void*,void*,char,void*);
typedef int(*egt_t)(void*,void**);
#define G(t,n) (t)(void*)GetProcAddress(h,n)
int main(int argc,char**argv){
    HMODULE h=LoadLibraryA("AlgoMilan.dll");
    int _ppp=getenv("PPP")?atoi(getenv("PPP")):10; ((ppp_t)G(ppp_t,"ppp_param_init"))(_ppp);
    ((ic_t)G(ic_t,"preprocess_init_calidata"))();
    ld_t load=G(ld_t,"preprocess_load_calidata");
    void*c1=calloc(1,0x184ac);void*c2=calloc(1,0xa004);unsigned s1=0x184ac,s2=0xa004;
    ((sc_t)G(sc_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2); load(c1,s1,c2,s2);
    /* preprocessor_init: build Kr from the no-finger base */
    unsigned short* basefr=calloc(1,FB);
    { FILE*bf=fopen(argv[1],"rb"); if(bf){fread(basefr,1,FB,bf);fclose(bf);} }
    char* cfg=calloc(1,0x40); P(cfg,0x18,basefr); U32(cfg,0x24,W); U32(cfg,0x28,H);
    ((ppi_t)G(ppi_t,"preprocessor_init"))(cfg);
    int _md=getenv("MODE")?atoi(getenv("MODE")):1; ((sm_t)G(sm_t,"preprocess_set_mode"))(_md);
    pp_t preprocessor=G(pp_t,"preprocessor");
    /* preprocess the frame */
    unsigned short* raw=calloc(1,FB); unsigned char* enh=calloc(1,IB);
    { FILE*f=fopen(argv[2],"rb"); if(f){fread(raw,1,FB,f);fclose(f);} }
    char*in=calloc(1,SS);char*out=calloc(1,SS);char*qc=calloc(1,0x20);
    void*a1=calloc(1,0x8000);void*a2=calloc(1,0x8000);
    P(in,0,raw);U16(in,8,W);U16(in,0xa,H);U32(in,0x14,FB);U16(in,0x18,1);
    P(out,0,enh);U16(out,8,W);U16(out,0xa,H);U32(out,0x14,IB);
    int _f1=getenv("FLAG1")?atoi(getenv("FLAG1")):0; int _f2=getenv("FLAG2")?atoi(getenv("FLAG2")):0; preprocessor(in,a1,a2,out,qc,(char)_f1,(char)_f2);
    U8(out,0xe,8);U8(out,0xf,1);U16(out,0x18,1);
    /* enroll this one frame */
    void*ctx=((es_t)G(es_t,"enrolStart"))();
    void*e2=calloc(1,0x8000);void*e3=calloc(1,0x8000);char*meta=calloc(1,0x80);
    ((eai_t)G(eai_t,"enrolAddImage"))(ctx,out,e2,e3,0,meta);
    void*tpl=NULL; ((egt_t)G(egt_t,"enrolGetTemplate"))(ctx,&tpl);
    unsigned char* inner=*(unsigned char**)tpl;
    unsigned char* sub=*(unsigned char**)(inner+0x28);
    int f0=*(int*)(sub+0xf0);
    unsigned char* feats=*(unsigned char**)(sub+0xf8);
    unsigned quality=(unsigned char)out[0x28], coverage=(unsigned char)out[0x29];
    /* header: f0, quality, coverage; then f0*0x38 feature bytes */
    FILE* of=fopen(argv[3],"wb");
    unsigned hdr[3]={ (unsigned)f0, quality, coverage };
    fwrite(hdr,4,3,of); fwrite(feats,0x38,f0,of); fclose(of);
    printf("%s: f0=%d quality=%u coverage=%u -> %s\n", argv[2], f0, quality, coverage, argv[3]);
    return 0;
}
