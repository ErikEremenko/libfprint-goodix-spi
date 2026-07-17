// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * AlgoMilan enroll harness (Wine).  Drives the full enroll pipeline on real
 * captured frames and dumps the resulting template — exercising the feature
 * extraction (0x1800152b0) and register/study (0x180017160) core.
 *
 *   enrol.exe <offset> frame1.raw frame2.raw ...
 *
 * Build: x86_64-w64-mingw32-gcc -O2 enroll.c -o enroll.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define W 64
#define H 80
#define FRAME_BYTES (2 * W * H)
#define STRUCT_SZ 0x40
#define PUT_PTR(s,o,v) (*(void**)((char*)(s)+(o))=(void*)(v))
#define PUT_U16(s,o,v) (*(unsigned short*)((char*)(s)+(o))=(unsigned short)(v))
#define PUT_U32(s,o,v) (*(unsigned*)((char*)(s)+(o))=(unsigned)(v))

typedef int   (*ppp_param_init_t)(int);
typedef int   (*init_calidata_t)(void);
typedef int   (*save_calidata_t)(void*,unsigned*,void*,unsigned*);
typedef int   (*load_calidata_t)(void*,unsigned,void*,unsigned);
typedef int   (*set_mode_t)(int);
typedef void* (*enrolStart_t)(void);
typedef int   (*enrolAddImage_t)(void*,void*,void*,void*,char,void*);
typedef int   (*enrolGetTemplate_t)(void*,void**);
typedef int   (*enrolFinish_t)(void*);
#define GET(t,n) (t)(void*)GetProcAddress(h,n)

int main(int argc, char** argv)
{
    HMODULE h = LoadLibraryA("AlgoMilan.dll");
    if (!h) { printf("load fail %lu\n",(unsigned long)GetLastError()); return 1; }
    /* argv[1]=baseline.raw  argv[2]=C  argv[3..]=finger frames.
     * offset_map[i] = baseline[i] + C  (calibration: calibrated = offset - raw). */
    const char* base_path = (argc>1)?argv[1]:NULL;
    unsigned C = (argc>2)?(unsigned)atoi(argv[2]):6000;

    /* ---- init (same bootstrap as loader.c) ---- */
    ((ppp_param_init_t)GET(ppp_param_init_t,"ppp_param_init"))(10);
    ((init_calidata_t)GET(init_calidata_t,"preprocess_init_calidata"))();
    unsigned short* offmap=(unsigned short*)((char*)h+0xc14d0+0x9924);
    { unsigned short base[W*H]; for(int i=0;i<W*H;i++) base[i]=0;
      FILE* bf = base_path?fopen(base_path,"rb"):NULL;
      if(bf){ fread(base,2,W*H,bf); fclose(bf); }
      for(int i=0;i<W*H;i++) offmap[i]=(unsigned short)(base[i]+C);
      printf("offset = baseline(%s)+%u\n", base_path?base_path:"0", C); }
    void* c1=calloc(1,0x184ac); void* c2=calloc(1,0xa004);
    unsigned s1=0x184ac,s2=0xa004;
    ((save_calidata_t)GET(save_calidata_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2);
    ((load_calidata_t)GET(load_calidata_t,"preprocess_load_calidata"))(c1,s1,c2,s2);
    ((set_mode_t)GET(set_mode_t,"preprocess_set_mode"))(1);
    printf("init done\n");

    /* ---- enroll ---- */
    enrolStart_t enrolStart = GET(enrolStart_t,"enrolStart");
    enrolAddImage_t enrolAddImage = GET(enrolAddImage_t,"enrolAddImage");
    enrolGetTemplate_t enrolGetTemplate = GET(enrolGetTemplate_t,"enrolGetTemplate");
    enrolFinish_t enrolFinish = GET(enrolFinish_t,"enrolFinish");

    void* ctx = enrolStart();
    printf("enrolStart -> ctx=%p\n", ctx);
    if (!ctx) return 2;

    char* in = (char*)calloc(1,STRUCT_SZ);
    void* a2 = calloc(1,0x8000);
    void* a3 = calloc(1,0x8000);
    char* meta = (char*)calloc(1,0x40);
    unsigned short* frame = (unsigned short*)calloc(1,FRAME_BYTES);

    for (int f=3; f<argc; f++) {
        FILE* ff=fopen(argv[f],"rb");
        if(!ff){printf("skip %s\n",argv[f]);continue;}
        fread(frame,1,FRAME_BYTES,ff); fclose(ff);
        PUT_PTR(in,0x00,frame); PUT_U16(in,0x08,W); PUT_U16(in,0x0a,H);
        *(unsigned char*)(in+0x0e)=8;   /* depth (enrolAddImage requires ==8) */
        *(unsigned char*)(in+0x0f)=1;   /* channels (==1) */
        PUT_U32(in,0x14,FRAME_BYTES); PUT_U16(in,0x18,1);
        int r = enrolAddImage(ctx, in, a2, a3, 0, meta);
        unsigned samples = *(unsigned short*)((char*)ctx+0xa);
        unsigned progress = *(unsigned*)((char*)ctx+0xc);
        printf("addImage %-32s ret=0x%x samples=%u progress=%u%%\n",
               argv[f], r, samples, progress);
    }

    void* tpl = NULL;
    int gr = enrolGetTemplate(ctx, &tpl);
    printf("enrolGetTemplate ret=0x%x tpl=%p\n", gr, tpl);
    if (tpl) {
        /* peek the first bytes of the template object + follow one indirection */
        unsigned char* t = (unsigned char*)tpl;
        printf("tpl[0..32]: "); for(int i=0;i<32;i++) printf("%02x ",t[i]); printf("\n");
        void* inner = *(void**)tpl;
        printf("*tpl=%p\n", inner);
        if (inner) {
            unsigned char* b=(unsigned char*)inner;
            printf("*tpl[0..32]: "); for(int i=0;i<32;i++) printf("%02x ",b[i]); printf("\n");
        }
    }
    enrolFinish(ctx);
    printf("done\n");
    return 0;
}
