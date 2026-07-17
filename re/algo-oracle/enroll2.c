// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * AlgoMilan enroll harness v2 (Wine) — CORRECT pipeline.
 *
 * enrolAddImage does NOT preprocess internally: it reads quality@0x28 /
 * coverage@0x29 straight off the input image struct and goes to feature
 * extraction.  So the caller must run preprocessor() FIRST and feed the
 * enhanced 8-bit output struct (with quality/coverage populated) to enroll.
 *
 * Pipeline per raw frame:
 *   preprocessor(raw16, a1, a2, enhanced8, qc, 0, 0)
 *     -> enhanced8: buf(8bpp), w@8, h@0xa, coverage@0x28, quality@0x29
 *   set enhanced8 depth@0x0e=8, channels@0x0f=1, frame_count@0x18=1
 *   enrolAddImage(ctx, enhanced8, a2, a3, 0, meta)
 *
 *   enroll2.exe <baseline.raw> <C> <frame1.raw> ...
 *   offset_map[i] = baseline[i] + C   (calibration: calibrated = offset - raw)
 *
 * Build: x86_64-w64-mingw32-gcc -O2 enroll2.c -o enroll2.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define W 64
#define H 80
#define FRAME_BYTES (2 * W * H)   /* 16-bpp raw input: 10240 */
#define IMG_BYTES   (W * H)       /* 8-bit enhanced:    5120 */
#define STRUCT_SZ 0x40
#define PUT_PTR(s,o,v) (*(void**)((char*)(s)+(o))=(void*)(v))
#define PUT_U16(s,o,v) (*(unsigned short*)((char*)(s)+(o))=(unsigned short)(v))
#define PUT_U32(s,o,v) (*(unsigned*)((char*)(s)+(o))=(unsigned)(v))
#define PUT_U8(s,o,v)  (*(unsigned char*)((char*)(s)+(o))=(unsigned char)(v))

typedef int   (*ppp_param_init_t)(int);
typedef int   (*init_calidata_t)(void);
typedef int   (*save_calidata_t)(void*,unsigned*,void*,unsigned*);
typedef int   (*load_calidata_t)(void*,unsigned,void*,unsigned);
typedef int   (*set_mode_t)(int);
typedef int   (*preprocessor_t)(void*,void*,void*,void*,void*,char,char);
typedef void* (*enrolStart_t)(void);
typedef int   (*enrolAddImage_t)(void*,void*,void*,void*,char,void*);
typedef int   (*enrolGetTemplate_t)(void*,void**);
typedef int   (*enrolFinish_t)(void*);
#define GET(t,n) (t)(void*)GetProcAddress(h,n)

int main(int argc, char** argv)
{
    HMODULE h = LoadLibraryA("AlgoMilan.dll");
    if (!h) { printf("load fail %lu\n",(unsigned long)GetLastError()); return 1; }
    const char* base_path = (argc>1)?argv[1]:NULL;
    unsigned C = (argc>2)?(unsigned)atoi(argv[2]):6000;

    /* ---- init + calibration poke ---- */
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

    preprocessor_t     preprocessor  = GET(preprocessor_t,"preprocessor");
    enrolStart_t       enrolStart    = GET(enrolStart_t,"enrolStart");
    enrolAddImage_t    enrolAddImage = GET(enrolAddImage_t,"enrolAddImage");
    enrolGetTemplate_t enrolGetTemplate = GET(enrolGetTemplate_t,"enrolGetTemplate");
    enrolFinish_t      enrolFinish   = GET(enrolFinish_t,"enrolFinish");

    void* ctx = enrolStart();
    printf("enrolStart -> ctx=%p\n", ctx);
    if (!ctx) return 2;

    /* raw input + enhanced output structs + scratch */
    unsigned short* raw = (unsigned short*)calloc(1,FRAME_BYTES);
    unsigned char*  enh = (unsigned char*)calloc(1,IMG_BYTES);
    char* in  = (char*)calloc(1,STRUCT_SZ);
    char* out = (char*)calloc(1,STRUCT_SZ);
    char* qc  = (char*)calloc(1,0x20);
    void* pa1 = calloc(1,0x8000);
    void* pa2 = calloc(1,0x8000);
    void* ea2 = calloc(1,0x8000);
    void* ea3 = calloc(1,0x8000);
    char* meta = (char*)calloc(1,0x40);

    for (int f=3; f<argc; f++) {
        FILE* ff=fopen(argv[f],"rb");
        if(!ff){printf("skip %s\n",argv[f]);continue;}
        memset(raw,0,FRAME_BYTES);
        fread(raw,1,FRAME_BYTES,ff); fclose(ff);

        /* --- preprocess the raw frame --- */
        memset(enh,0,IMG_BYTES);
        PUT_PTR(in,0x00,raw);  PUT_U16(in,0x08,W); PUT_U16(in,0x0a,H);
        PUT_U32(in,0x14,FRAME_BYTES); PUT_U16(in,0x18,1);
        PUT_PTR(out,0x00,enh); PUT_U16(out,0x08,W); PUT_U16(out,0x0a,H);
        PUT_U32(out,0x14,IMG_BYTES);
        int pr = preprocessor(in, pa1, pa2, out, qc, 0, 0);
        unsigned cov = (unsigned char)out[0x28];
        unsigned qual= (unsigned char)out[0x29];

        /* --- mark enhanced struct as an 8-bpp image for enroll --- */
        PUT_U8(out,0x0e,8);   /* depth */
        PUT_U8(out,0x0f,1);   /* channels */
        PUT_U16(out,0x18,1);  /* frame_count != 0 */

        int r = enrolAddImage(ctx, out, ea2, ea3, 0, meta);
        unsigned samples = *(unsigned short*)((char*)ctx+0xa);
        unsigned progress= *(unsigned*)((char*)ctx+0xc);
        printf("%-28s pre=0x%x cov=%u qual=%u | add=0x%x samples=%u prog=%u%%\n",
               argv[f], pr, cov, qual, r, samples, progress);
    }

    void* tpl=NULL;
    int gr = enrolGetTemplate(ctx,&tpl);
    printf("enrolGetTemplate ret=0x%x tpl=%p\n", gr, tpl);
    enrolFinish(ctx);
    printf("done\n");
    return 0;
}
