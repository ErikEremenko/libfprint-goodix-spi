// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * AlgoMilan full-init harness — replicate the Windows engine init sequence:
 *   ppp_param_init -> init_calidata -> load_calidata
 *   -> preprocessor_init(config{ +0x18: base-frame, +0x24/+0x28: dims })  // builds Kr/ImageBase
 *   -> set_mode -> enroll/identify
 *
 *   fullinit.exe <basefr.raw> <N_gal> <galFrames...> <probeFrame>
 *   env REAL=1  -> load real_b1.bin/real_b2.bin (needs patched DLL)
 *
 * Build: x86_64-w64-mingw32-gcc -O2 fullinit.c -o fullinit.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#define W 64
#define H 80
#define FRAME_BYTES (2*W*H)
#define IMG_BYTES (W*H)
#define STRUCT_SZ 0x40
#define PUT_PTR(s,o,v) (*(void**)((char*)(s)+(o))=(void*)(v))
#define PUT_U16(s,o,v) (*(unsigned short*)((char*)(s)+(o))=(unsigned short)(v))
#define PUT_U32(s,o,v) (*(unsigned*)((char*)(s)+(o))=(unsigned)(v))
#define PUT_U8(s,o,v)  (*(unsigned char*)((char*)(s)+(o))=(unsigned char)(v))
typedef int (*ppp_t)(int); typedef int (*ic_t)(void);
typedef int (*sc_t)(void*,unsigned*,void*,unsigned*);
typedef int (*ld_t)(void*,unsigned,void*,unsigned);
typedef int (*sm_t)(int);
typedef int (*ppi_t)(void*);          /* preprocessor_init(config) */
typedef int (*pp_t)(void*,void*,void*,void*,void*,char,char);
typedef void* (*es_t)(void);
typedef int (*eai_t)(void*,void*,void*,void*,char,void*);
typedef int (*egt_t)(void*,void**);
typedef int (*idi_t)(void*,void*,void*,int,int*,int*,void*,int,int,void*,int);
#define GET(t,n) (t)(void*)GetProcAddress(h,n)
static HMODULE h; static pp_t preprocessor;
static LONG CALLBACK veh(EXCEPTION_POINTERS* ep){
    EXCEPTION_RECORD* er=ep->ExceptionRecord;
    if(er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION){
        printf("\n*** AV faultAddr=0x%llx rip=0x%llx\n",
            (unsigned long long)(er->NumberParameters>=2?er->ExceptionInformation[1]:0),
            (unsigned long long)ep->ContextRecord->Rip); fflush(stdout); ExitProcess(7);}
    return EXCEPTION_CONTINUE_SEARCH;
}
static void preprocess_one(const char* p, char* out, unsigned char* enh,
                           void* a1,void* a2,char* qc,char* in,unsigned short* raw){
    FILE* f=fopen(p,"rb"); memset(raw,0,FRAME_BYTES);
    if(f){fread(raw,1,FRAME_BYTES,f);fclose(f);}
    memset(enh,0,IMG_BYTES);
    PUT_PTR(in,0,raw);PUT_U16(in,8,W);PUT_U16(in,0xa,H);PUT_U32(in,0x14,FRAME_BYTES);PUT_U16(in,0x18,1);
    PUT_PTR(out,0,enh);PUT_U16(out,8,W);PUT_U16(out,0xa,H);PUT_U32(out,0x14,IMG_BYTES);
    preprocessor(in,a1,a2,out,qc,0,0);
    PUT_U8(out,0xe,8);PUT_U8(out,0xf,1);PUT_U16(out,0x18,1);
}
int main(int argc,char**argv){
    h=LoadLibraryA("AlgoMilan.dll"); AddVectoredExceptionHandler(1,veh);
    const char* basep=argv[1]; int ngal=atoi(argv[2]);
    ((ppp_t)GET(ppp_t,"ppp_param_init"))(10);
    ((ic_t)GET(ic_t,"preprocess_init_calidata"))();
    ld_t load=GET(ld_t,"preprocess_load_calidata");
    if(getenv("REAL")){
        FILE* f1=fopen("real_b1.bin","rb"),*f2=fopen("real_b2.bin","rb");
        void* b1=calloc(1,0x184ac); void* b2=calloc(1,0xa004);
        fread(b1,1,0x184ac,f1);fread(b2,1,0xa004,f2);fclose(f1);fclose(f2);
        printf("load_calidata(REAL) ret=0x%x\n",load(b1,0x184ac,b2,0xa004));
    } else {
        void* c1=calloc(1,0x184ac);void* c2=calloc(1,0xa004);unsigned s1=0x184ac,s2=0xa004;
        ((sc_t)GET(sc_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2);
        load(c1,s1,c2,s2); printf("load_calidata(default) done\n");
    }
    /* ---- preprocessor_init: build Kr/ImageBase from a base frame ---- */
    unsigned short* basefr=calloc(1,FRAME_BYTES);
    { FILE* bf=fopen(basep,"rb"); if(bf){fread(basefr,1,FRAME_BYTES,bf);fclose(bf);} }
    char* cfg=calloc(1,0x40);
    PUT_PTR(cfg,0x18,basefr); PUT_U32(cfg,0x24,W); PUT_U32(cfg,0x28,H);
    ppi_t ppinit=GET(ppi_t,"preprocessor_init");
    printf("preprocessor_init=%p\n",(void*)ppinit);
    unsigned char* caliData=(unsigned char*)((char*)h+0xc14d0);
    unsigned short* m4a=(unsigned short*)(caliData+4);
    unsigned short* m99a=(unsigned short*)(caliData+0x9924);
    printf("BEFORE ppinit: caliData+4=[%u %u %u %u] +0x9924=[%u %u %u %u]\n",
        m4a[0],m4a[1],m4a[2],m4a[3], m99a[0],m99a[1],m99a[2],m99a[3]);
    int ir=ppinit(cfg);
    printf("preprocessor_init(base=%s) ret=0x%x\n", basep, ir);
    printf("AFTER  ppinit: caliData+4=[%u %u %u %u] +0x9924=[%u %u %u %u]\n",
        m4a[0],m4a[1],m4a[2],m4a[3], m99a[0],m99a[1],m99a[2],m99a[3]);
    ((sm_t)GET(sm_t,"preprocess_set_mode"))(1);

    preprocessor=GET(pp_t,"preprocessor");
    es_t es=GET(es_t,"enrolStart"); eai_t eai=GET(eai_t,"enrolAddImage");
    egt_t egt=GET(egt_t,"enrolGetTemplate");
    idi_t idi=GET(idi_t,"identifyImageWrapper");
    unsigned short* raw=calloc(1,FRAME_BYTES); unsigned char* enh=calloc(1,IMG_BYTES);
    char* in=calloc(1,STRUCT_SZ);char* out=calloc(1,STRUCT_SZ);char* qc=calloc(1,0x20);
    void* a1=calloc(1,0x8000);void* a2=calloc(1,0x8000);
    void* e2=calloc(1,0x8000);void* e3=calloc(1,0x8000);char* meta=calloc(1,0x80);

    void* ctx=es();
    for(int i=0;i<ngal;i++){
        preprocess_one(argv[3+i],out,enh,a1,a2,qc,in,raw);
        int r=eai(ctx,out,e2,e3,0,meta);
        printf("  gal[%d] %s cov=%u qual=%u add=0x%x samples=%u\n",i,argv[3+i],
            (unsigned char)out[0x28],(unsigned char)out[0x29],r,*(unsigned short*)((char*)ctx+0xa));
    }
    void* gtpl=NULL; egt(ctx,&gtpl);
    /* Inspect the enrolled template's per-sub minutiae counts (sub+0xf0), and the
     * descriptor fields the matcher's correspondence gate reads (+0x10c/+0x110). */
    { unsigned char* inner=*(unsigned char**)gtpl;
      unsigned subcnt=*(unsigned*)(inner+0x1c);
      printf("gallery inner=%p subcount[0x1c]=%u\n", inner, subcnt);
      for(unsigned i=0;i<subcnt && i<12;i++){
        unsigned char* sub=*(unsigned char**)(inner+0x28+i*8);
        if(!sub){printf("  sub[%u]=NULL\n",i);continue;}
        printf("  sub[%u]=%p f0(minutiae?)=%d [0x10c]=%d [0x110]=%d [0x14c]=%d\n",
          i, sub, *(int*)(sub+0xf0), *(int*)(sub+0x10c), *(int*)(sub+0x110), *(int*)(sub+0x14c));
      }
    }
    unsigned char* gsub0 = *(unsigned char**)((*(unsigned char**)gtpl)+0x28);
    void* gal[4]={gtpl};
    const char* probe=argv[argc-1];
    preprocess_one(probe,out,enh,a1,a2,qc,in,raw);
    printf("probe %s cov=%u qual=%u\n",probe,(unsigned char)out[0x28],(unsigned char)out[0x29]);
    int idx=-999,score=-999;
    int rc=idi(out,NULL,gal,1,&idx,&score,meta,0,0,NULL,0);
    printf("==> identify rc=0x%x idx=%d score=%d %s\n",rc,idx,score,idx>=0?"MATCH":"");

    /* Probe descriptor lives in the .data global at base+0xbbbd0 (identifyImage's
     * extractor writes a pointer there; the scorer reads it). Diff its feature
     * array against gallery sub[0]'s for the SAME frame. */
    unsigned char* pd = *(unsigned char**)((char*)h + 0xbbbd0);
    printf("probeDesc=%p  gallery sub[0]=%p\n", pd, gsub0);
    if (pd && gsub0) {
        printf("  probe   f0=%d [0x108]=%d [0x10c]=%d [0x110]=%d [0x14c]=%d\n",
            *(int*)(pd+0xf0), *(int*)(pd+0x108), *(int*)(pd+0x10c), *(int*)(pd+0x110), *(int*)(pd+0x14c));
        printf("  gallery f0=%d [0x108]=%d [0x10c]=%d [0x110]=%d [0x14c]=%d\n",
            *(int*)(gsub0+0xf0), *(int*)(gsub0+0x108), *(int*)(gsub0+0x10c), *(int*)(gsub0+0x110), *(int*)(gsub0+0x14c));
        /* first 6 features' leading words (x/y/angle?) from the gallery */
        { short* g=(short*)*(void**)(gsub0+0xf8);
          printf("  gallery feat words[0..3] for 6 feats:");
          for(int f=0;f<6;f++){short*ff=(short*)((char*)g+f*0x38);
            printf(" [%d,%d,%d,%d]",ff[0],ff[1],ff[2],ff[3]);}
          printf("\n"); }
        unsigned char* pf=*(unsigned char**)(pd+0xf8);
        unsigned char* gf=*(unsigned char**)(gsub0+0xf8);
        if(pf&&gf){
            int n=*(int*)(pd+0xf0); int bytes=n*0x38;   /* stride 0x38 per feature */
            int diff=0, first=-1;
            for(int i=0;i<bytes;i++) if(pf[i]!=gf[i]){diff++; if(first<0)first=i;}
            printf("  FULL feature array (%d feats x0x38 = %d bytes): %d differ, firstDiff=%d (feat %d)\n",
                   n, bytes, diff, first, first>=0?first/0x38:-1);
        }
    }
    return 0;
}
