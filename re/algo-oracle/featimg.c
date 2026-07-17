// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Extract AlgoMilan features from an ARBITRARY 8-bit enhanced image (bypasses the
 * preprocessor), so we can probe the enhanced-image -> descriptor step directly.
 *   featimg.exe <enh8.bin (W*H bytes)> <out.feat>   [QUAL env, COV env]
 * Build: x86_64-w64-mingw32-gcc -O2 featimg.c -o featimg.exe */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#define W 64
#define H 80
#define IB (W*H)
#define SS 0x40
#define P(s,o,v)   (*(void**)((char*)(s)+(o))=(void*)(v))
#define U16(s,o,v) (*(unsigned short*)((char*)(s)+(o))=(unsigned short)(v))
#define U32(s,o,v) (*(unsigned*)((char*)(s)+(o))=(unsigned)(v))
#define U8(s,o,v)  (*(unsigned char*)((char*)(s)+(o))=(unsigned char)(v))
typedef int (*ppp_t)(int); typedef int (*ic_t)(void);
typedef int (*sc_t)(void*,unsigned*,void*,unsigned*); typedef int (*ld_t)(void*,unsigned,void*,unsigned);
typedef int (*sm_t)(int);
typedef void*(*es_t)(void); typedef int (*eai_t)(void*,void*,void*,void*,char,void*);
typedef int (*egt_t)(void*,void**);
#define G(t,n) (t)(void*)GetProcAddress(h,n)
static HMODULE h;
int main(int argc,char**argv){
    if(argc<3){printf("usage: featimg <enh8.bin> <out.feat>\n");return 2;}
    h=LoadLibraryA("AlgoMilan.dll");
    ((ppp_t)G(ppp_t,"ppp_param_init"))(10);
    ((ic_t)G(ic_t,"preprocess_init_calidata"))();
    ld_t load=G(ld_t,"preprocess_load_calidata");
    void*c1=calloc(1,0x184ac);void*c2=calloc(1,0xa004);unsigned s1=0x184ac,s2=0xa004;
    ((sc_t)G(sc_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2); load(c1,s1,c2,s2);
    ((sm_t)G(sm_t,"preprocess_set_mode"))(1);
    es_t enrolStart=G(es_t,"enrolStart"); eai_t enrolAddImage=G(eai_t,"enrolAddImage");
    egt_t enrolGetTemplate=G(egt_t,"enrolGetTemplate");
    unsigned char* enh=calloc(1,IB);
    { FILE*f=fopen(argv[1],"rb"); if(f){ if(fread(enh,1,IB,f)){} fclose(f);} }
    int qual=getenv("QUAL")?atoi(getenv("QUAL")):85;
    int cov =getenv("COV") ?atoi(getenv("COV")) :100;
    char* out=calloc(1,SS);
    P(out,0,enh); U16(out,8,W); U16(out,0xa,H); U32(out,0x14,IB);
    U8(out,0x28,qual); U8(out,0x29,cov);          /* quality / coverage */
    U8(out,0x0e,8); U8(out,0x0f,1); U16(out,0x18,1);
    void* ea2=calloc(1,0x8000); void* ea3=calloc(1,0x8000); char* meta=calloc(1,0x80);
    void*ctx=enrolStart();
    int r=enrolAddImage(ctx,out,ea2,ea3,0,meta);
    void*tpl=NULL; enrolGetTemplate(ctx,&tpl);
    unsigned char* inner=tpl?*(unsigned char**)tpl:0;
    unsigned char* sub=inner?*(unsigned char**)(inner+0x28):0;
    int f0= sub?*(int*)(sub+0xf0):0;
    unsigned char* feats= sub?*(unsigned char**)(sub+0xf8):0;
    FILE* of=fopen(argv[2],"wb");
    unsigned hdr[3]={(unsigned)f0,(unsigned)qual,(unsigned)cov};
    fwrite(hdr,4,3,of); if(feats&&f0>0) fwrite(feats,0x38,f0,of); fclose(of);
    printf("add=0x%x f0=%d -> %s\n", r, f0, argv[2]);
    return 0;
}
