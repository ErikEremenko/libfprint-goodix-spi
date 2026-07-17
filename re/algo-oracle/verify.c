// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * AlgoMilan verify harness (Wine) — end-to-end matcher validation.
 *
 * Enrolls a GALLERY template from one set of frames and a PROBE template from a
 * held-out set (both via preprocess->enrolAddImage, the correct pipeline), then
 * calls identifytemplate(gallery, probe, &idx) to test genuine-accept.
 *
 *   verify.exe <baseline.raw> <C> <N_gallery> <f1..fN_gallery> <p1..pM_probe>
 *   first N frames after N_gallery build the gallery; the rest are the probe.
 *
 * Build: x86_64-w64-mingw32-gcc -O2 verify.c -o verify.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define W 64
#define H 80
#define FRAME_BYTES (2*W*H)
#define IMG_BYTES   (W*H)
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
typedef int   (*identifytemplate_t)(void*,void*,void*,int*);  /* result -> r9 (4th arg) */
#define GET(t,n) (t)(void*)GetProcAddress(h,n)

static HMODULE h;
static preprocessor_t preprocessor;
static enrolStart_t enrolStart;
static enrolAddImage_t enrolAddImage;
static enrolGetTemplate_t enrolGetTemplate;

/* preprocess+enroll a list of frames, return the template handle (out param). */
static void* build_template(char** frames, int nframes, const char* tag)
{
    void* ctx = enrolStart();
    if (!ctx) { printf("[%s] enrolStart failed\n", tag); return NULL; }
    unsigned short* raw = calloc(1,FRAME_BYTES);
    unsigned char*  enh = calloc(1,IMG_BYTES);
    char* in=calloc(1,STRUCT_SZ); char* out=calloc(1,STRUCT_SZ); char* qc=calloc(1,0x20);
    void* pa1=calloc(1,0x8000); void* pa2=calloc(1,0x8000);
    void* ea2=calloc(1,0x8000); void* ea3=calloc(1,0x8000); char* meta=calloc(1,0x40);

    for (int i=0;i<nframes;i++){
        FILE* ff=fopen(frames[i],"rb");
        if(!ff){printf("[%s] skip %s\n",tag,frames[i]);continue;}
        memset(raw,0,FRAME_BYTES); fread(raw,1,FRAME_BYTES,ff); fclose(ff);
        memset(enh,0,IMG_BYTES);
        PUT_PTR(in,0,raw); PUT_U16(in,8,W); PUT_U16(in,0xa,H);
        PUT_U32(in,0x14,FRAME_BYTES); PUT_U16(in,0x18,1);
        PUT_PTR(out,0,enh); PUT_U16(out,8,W); PUT_U16(out,0xa,H); PUT_U32(out,0x14,IMG_BYTES);
        preprocessor(in,pa1,pa2,out,qc,0,0);
        PUT_U8(out,0x0e,8); PUT_U8(out,0x0f,1); PUT_U16(out,0x18,1);
        int r = enrolAddImage(ctx,out,ea2,ea3,0,meta);
        unsigned samples=*(unsigned short*)((char*)ctx+0xa);
        unsigned prog=*(unsigned*)((char*)ctx+0xc);
        printf("[%s] %-28s add=0x%x samples=%u prog=%u%%\n",tag,frames[i],r,samples,prog);
    }
    void* tpl=NULL;
    int gr=enrolGetTemplate(ctx,&tpl);
    printf("[%s] enrolGetTemplate ret=0x%x tpl=%p\n",tag,gr,tpl);
    return tpl;   /* leave ctx alive (don't enrolFinish — that frees the template) */
}

int main(int argc, char** argv)
{
    h = LoadLibraryA("AlgoMilan.dll");
    if (!h){printf("load fail\n");return 1;}
    const char* base_path=argv[1];
    unsigned C=(unsigned)atoi(argv[2]);
    int ngal=atoi(argv[3]);

    ((ppp_param_init_t)GET(ppp_param_init_t,"ppp_param_init"))(10);
    ((init_calidata_t)GET(init_calidata_t,"preprocess_init_calidata"))();
    unsigned short* offmap=(unsigned short*)((char*)h+0xc14d0+0x9924);
    { unsigned short base[W*H]; for(int i=0;i<W*H;i++) base[i]=0;
      FILE* bf=fopen(base_path,"rb"); if(bf){fread(base,2,W*H,bf);fclose(bf);}
      for(int i=0;i<W*H;i++) offmap[i]=(unsigned short)(base[i]+C); }
    void* c1=calloc(1,0x184ac); void* c2=calloc(1,0xa004); unsigned s1=0x184ac,s2=0xa004;
    ((save_calidata_t)GET(save_calidata_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2);
    ((load_calidata_t)GET(load_calidata_t,"preprocess_load_calidata"))(c1,s1,c2,s2);
    ((set_mode_t)GET(set_mode_t,"preprocess_set_mode"))(1);

    preprocessor=GET(preprocessor_t,"preprocessor");
    enrolStart=GET(enrolStart_t,"enrolStart");
    enrolAddImage=GET(enrolAddImage_t,"enrolAddImage");
    enrolGetTemplate=GET(enrolGetTemplate_t,"enrolGetTemplate");
    identifytemplate_t identifytemplate=GET(identifytemplate_t,"identifytemplate");

    char** gal=&argv[4];
    void* gtpl=build_template(gal, ngal, "GAL");
    char** prb=&argv[4+ngal];
    int nprb=argc-(4+ngal);
    printf("--- probe frames: %d ---\n", nprb);
    void* ptpl=build_template(prb, nprb, "PRB");

    /* diagnostics: dump inner blob type + count for both templates */
    for (int k=0;k<2;k++){
        void* t = k?ptpl:gtpl; const char* nm = k?"PRB":"GAL";
        if(!t) continue;
        unsigned char* inner = *(unsigned char**)t;
        printf("[%s] inner=%p type[0]=%u count[0x1c]=%u  head: ",nm,inner,
               *(unsigned*)inner, *(unsigned*)(inner+0x1c));
        for(int i=0;i<16;i++) printf("%02x ",inner[i]); printf("\n");
    }
    if (gtpl){
        int idx=-999; int rc=identifytemplate(gtpl, gtpl, NULL, &idx);
        printf("==> SELF-match gal/gal  ret=0x%x idx=%d (%s)\n",rc,idx,idx>=0?"MATCH":"no-match");
    }
    if (gtpl && ptpl){
        int idx=-999;
        int rc=identifytemplate(gtpl, ptpl, NULL, &idx);
        printf("==> GENUINE gal/prb     ret=0x%x idx=%d (%s)\n",
               rc, idx, idx>=0 ? "MATCH" : "no-match");
    }
    printf("done\n");
    return 0;
}
