// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * AlgoMilan identify harness (Wine) — the real verify path.
 *
 * identifyImage(rcx=enhancedImage, rdx=?, r8=&galleryHandles[N], r9=N,
 *               arg5=&outIdx, arg6=&outScore, arg7=meta, arg8, arg9, ...)
 * loops N gallery templates, extracts the probe descriptor from the enhanced
 * image, scores vs each (scorer 0x18001d350); if score>0 -> *outIdx=i,*outScore.
 *
 *   identify.exe <baseline.raw> <C> <N_gallery> <galFrames...> <probeFrame>
 * last frame = probe; preceding N_gallery frames build the gallery template.
 *
 * Build: x86_64-w64-mingw32-gcc -O2 identify.c -o identify.exe
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
/* 9+ args; result index -> arg5, score -> arg6 */
typedef int   (*identifyImage_t)(void*,void*,void*,int,int*,int*,void*,int,int,void*,int);
#define GET(t,n) (t)(void*)GetProcAddress(h,n)

static HMODULE h;

/* Vectored exception handler: on execute-fault at address 0, read the return
 * address off the stack (= instruction right after the `call [nullfptr]`) and
 * report it as an RVA into AlgoMilan.dll, so we can find the missing init. */
static LONG CALLBACK veh(EXCEPTION_POINTERS* ep)
{
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR faultaddr = er->NumberParameters>=2 ? er->ExceptionInformation[1] : 0;
        ULONG_PTR kind      = er->NumberParameters>=1 ? er->ExceptionInformation[0] : 0; /* 8=execute */
        CONTEXT* c = ep->ContextRecord;
        ULONG_PTR base = (ULONG_PTR)h;
        ULONG_PTR ret  = *(ULONG_PTR*)c->Rsp;          /* pushed return addr */
        printf("\n*** ACCESS_VIOLATION code=0x%lx kind=%llu(8=exec) faultAddr=0x%llx\n",
               (unsigned long)er->ExceptionCode,(unsigned long long)kind,(unsigned long long)faultaddr);
        printf("    ExceptionAddress=%p  Rip=0x%llx (rva 0x%llx)\n",
               er->ExceptionAddress,(unsigned long long)c->Rip,(unsigned long long)(c->Rip-base));
        printf("    return addr on stack=0x%llx  => AlgoMilan call-site RVA=0x%llx\n",
               (unsigned long long)ret,(unsigned long long)(ret-base));
        printf("    regs: rcx=0x%llx rdx=0x%llx r8=0x%llx r9=0x%llx rax=0x%llx\n",
               (unsigned long long)c->Rcx,(unsigned long long)c->Rdx,(unsigned long long)c->R8,
               (unsigned long long)c->R9,(unsigned long long)c->Rax);
        fflush(stdout);
        ExitProcess(7);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static preprocessor_t preprocessor;
static enrolStart_t enrolStart;
static enrolAddImage_t enrolAddImage;
static enrolGetTemplate_t enrolGetTemplate;

static void preprocess_one(const char* path, char* out, unsigned char* enh,
                           void* pa1, void* pa2, char* qc, char* in, unsigned short* raw)
{
    FILE* ff=fopen(path,"rb");
    memset(raw,0,FRAME_BYTES);
    if(ff){fread(raw,1,FRAME_BYTES,ff);fclose(ff);}
    memset(enh,0,IMG_BYTES);
    PUT_PTR(in,0,raw); PUT_U16(in,8,W); PUT_U16(in,0xa,H);
    PUT_U32(in,0x14,FRAME_BYTES); PUT_U16(in,0x18,1);
    PUT_PTR(out,0,enh); PUT_U16(out,8,W); PUT_U16(out,0xa,H); PUT_U32(out,0x14,IMG_BYTES);
    preprocessor(in,pa1,pa2,out,qc,0,0);
    PUT_U8(out,0x0e,8); PUT_U8(out,0x0f,1); PUT_U16(out,0x18,1);
}

int main(int argc, char** argv)
{
    h = LoadLibraryA("AlgoMilan.dll");
    if(!h){printf("load fail\n");return 1;}
    AddVectoredExceptionHandler(1, veh);
    printf("AlgoMilan base=%p\n",(void*)h);
    const char* base_path=argv[1];
    unsigned C=(unsigned)atoi(argv[2]);
    int ngal=atoi(argv[3]);

    ((ppp_param_init_t)GET(ppp_param_init_t,"ppp_param_init"))(10);
    ((init_calidata_t)GET(init_calidata_t,"preprocess_init_calidata"))();
    load_calidata_t load=GET(load_calidata_t,"preprocess_load_calidata");
    /* If the REAL factory calidata blobs are present, load them directly (this is
     * the exact Windows calibration). Otherwise fall back to synthetic poke. */
    FILE* rf1=fopen("real_b1.bin","rb"); FILE* rf2=fopen("real_b2.bin","rb");
    if (rf1 && rf2) {
        void* b1=calloc(1,0x184ac); void* b2=calloc(1,0xa004);
        fread(b1,1,0x184ac,rf1); fread(b2,1,0xa004,rf2); fclose(rf1); fclose(rf2);
        int lr=load(b1,0x184ac,b2,0xa004);
        printf("loaded REAL factory calidata (load_calidata ret=0x%x)\n", lr);
    } else {
        if(rf1)fclose(rf1); if(rf2)fclose(rf2);
        unsigned short* offmap=(unsigned short*)((char*)h+0xc14d0+0x9924);
        { unsigned short base[W*H]; for(int i=0;i<W*H;i++) base[i]=0;
          FILE* bf=fopen(base_path,"rb"); if(bf){fread(base,2,W*H,bf);fclose(bf);}
          for(int i=0;i<W*H;i++) offmap[i]=(unsigned short)(base[i]+C); }
        { unsigned short* gainmap=(unsigned short*)((char*)h+0xc14d0+4);
          FILE* gf=fopen("synth_gain.raw","rb");
          if(gf){ unsigned short g[W*H]; size_t n=fread(g,2,W*H,gf); fclose(gf);
            for(int i=0;i<W*H;i++) gainmap[i]=g[i]; printf("poked gain (%zu)\n",n); } }
        void* c1=calloc(1,0x184ac); void* c2=calloc(1,0xa004); unsigned s1=0x184ac,s2=0xa004;
        ((save_calidata_t)GET(save_calidata_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2);
        load(c1,s1,c2,s2);
        printf("using synthetic calibration (baseline+%u)\n", C);
    }
    ((set_mode_t)GET(set_mode_t,"preprocess_set_mode"))(1);

    preprocessor=GET(preprocessor_t,"preprocessor");
    enrolStart=GET(enrolStart_t,"enrolStart");
    enrolAddImage=GET(enrolAddImage_t,"enrolAddImage");
    enrolGetTemplate=GET(enrolGetTemplate_t,"enrolGetTemplate");
    identifyImage_t identifyImage=GET(identifyImage_t,"identifyImageWrapper");
    printf("identifyImageWrapper=%p\n",(void*)identifyImage);

    unsigned short* raw=calloc(1,FRAME_BYTES);
    unsigned char* enh=calloc(1,IMG_BYTES);
    char* in=calloc(1,STRUCT_SZ); char* out=calloc(1,STRUCT_SZ); char* qc=calloc(1,0x20);
    void* pa1=calloc(1,0x8000); void* pa2=calloc(1,0x8000);
    void* ea2=calloc(1,0x8000); void* ea3=calloc(1,0x8000); char* meta=calloc(1,0x80);

    /* ---- build gallery template from first ngal frames ---- */
    void* ctx=enrolStart();
    for(int i=0;i<ngal;i++){
        preprocess_one(argv[4+i], out, enh, pa1, pa2, qc, in, raw);
        enrolAddImage(ctx,out,ea2,ea3,0,meta);
    }
    unsigned gsamples=*(unsigned short*)((char*)ctx+0xa);
    void* gtpl=NULL; enrolGetTemplate(ctx,&gtpl);
    printf("gallery: samples=%u tpl=%p inner=%p\n", gsamples, gtpl, *(void**)gtpl);
    void* galArray[4]; galArray[0]=gtpl;   /* array of N=1 handle(s) */

    /* ---- probe: preprocess the last frame into an enhanced image ---- */
    const char* probe=argv[argc-1];
    preprocess_one(probe, out, enh, pa1, pa2, qc, in, raw);
    printf("probe %s: cov=%u qual=%u\n", probe,(unsigned char)out[0x28],(unsigned char)out[0x29]);

    /* rdx: the probe extractor's ctx/scratch — try NULL vs a big zeroed buffer */
    void* rdxbuf = calloc(1, 0x20000);
    for(int userdx=0; userdx<=1; userdx++){
        void* rdx = userdx ? rdxbuf : NULL;
        int idx=-999, score=-999;
        char* pmeta=calloc(1,0x80);
        *(unsigned char*)(out+0x0e)=8; *(unsigned char*)(out+0x0f)=1;
        int rc = identifyImage(out, rdx, galArray, 1, &idx, &score, pmeta, 0, 0, NULL, 0);
        printf("  rdx=%s -> rc=0x%x idx=%d score=%d %s\n",
               userdx?"buf":"NULL",rc,idx,score, idx>=0?"MATCH":"");
        free(pmeta);
    }
    printf("done\n");
    return 0;
}
