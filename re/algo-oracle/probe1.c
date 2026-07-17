// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Preprocess one frame, dump the full out-struct + enhanced-image stats.
 *   probe1.exe <base.raw> <frame.raw>
 * Build: x86_64-w64-mingw32-gcc -O2 probe1.c -o probe1.exe */
#include <windows.h>
#include <math.h>
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
    if(argc<3){printf("usage: probe1 <base> <frame>\n");return 2;}
    h=LoadLibraryA("AlgoMilan.dll");
    ((ppp_t)G(ppp_t,"ppp_param_init"))(10);
    ((ic_t)G(ic_t,"preprocess_init_calidata"))();
    ld_t load=G(ld_t,"preprocess_load_calidata");
    void*c1=calloc(1,0x184ac);void*c2=calloc(1,0xa004);unsigned s1=0x184ac,s2=0xa004;
    ((sc_t)G(sc_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2); load(c1,s1,c2,s2);
    unsigned short* basefr=calloc(1,FB);
    { FILE*bf=fopen(argv[1],"rb"); if(bf){if(fread(basefr,1,FB,bf)){}fclose(bf);} }
    char* cfg=calloc(1,0x40); P(cfg,0x18,basefr); U32(cfg,0x24,W); U32(cfg,0x28,H);
    ((ppi_t)G(ppi_t,"preprocessor_init"))(cfg);
    ((sm_t)G(sm_t,"preprocess_set_mode"))(1);
    pp_t preprocessor=G(pp_t,"preprocessor");
    unsigned short* raw=calloc(1,FB); unsigned char* enh=calloc(1,IB);
    { FILE*f=fopen(argv[2],"rb"); if(f){if(fread(raw,1,FB,f)){}fclose(f);} }
    char* in=calloc(1,SS); char* out=calloc(1,SS); char* qc=calloc(1,0x40);
    void* a1=calloc(1,0x8000); void* a2=calloc(1,0x8000);
    P(in,0,raw);U16(in,8,W);U16(in,0xa,H);U32(in,0x14,FB);U16(in,0x18,1);
    P(out,0,enh);U16(out,8,W);U16(out,0xa,H);U32(out,0x14,IB);
    preprocessor(in,a1,a2,out,qc,0,0);
    printf("out struct (0x00..0x40):\n");
    for(int i=0;i<0x40;i++){ printf("%02x ", (unsigned char)out[i]); if((i&15)==15)printf("\n"); }
    printf("qc struct (0x00..0x20):\n");
    for(int i=0;i<0x20;i++){ printf("%02x ", (unsigned char)qc[i]); if((i&15)==15)printf("\n"); }
    /* enhanced image stats */
    long sum=0; int mn=255,mx=0, nz=0;
    for(int i=0;i<IB;i++){ int v=enh[i]; sum+=v; if(v<mn)mn=v; if(v>mx)mx=v; if(v)nz++; }
    double mean=(double)sum/IB, var=0;
    for(int i=0;i<IB;i++){ double d=enh[i]-mean; var+=d*d; }
    printf("enh: mean=%.1f std=%.1f min=%d max=%d nonzero=%d/%d\n",
           mean, sqrt(var/IB), mn, mx, nz, IB);
    printf("out[0x28]=%u out[0x29]=%u out[0x2a]=%u out[0x2b]=%u\n",
           (unsigned char)out[0x28],(unsigned char)out[0x29],(unsigned char)out[0x2a],(unsigned char)out[0x2b]);
    return 0;
}
