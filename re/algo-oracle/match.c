// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * AlgoMilan MATCH harness (Wine) — the missing piece.
 *
 * Combines dumpfeat.c's WORKING preprocessing (preprocessor_init with a live
 * no-finger base, which is the only path that actually extracts features) with
 * identify.c's gallery-enroll + identifyImage probe.  verify.c/identify.c both
 * skipped preprocessor_init(base), so their preprocessor produced cov=0/qual=0
 * or degenerate templates that couldn't self-match.
 *
 *   match.exe <nofinger_base.raw> <N_gallery> <gal1..galN> <probe1..probeM>
 * First N frames = gallery; the rest are probed one-by-one against it.
 *
 * Build: x86_64-w64-mingw32-gcc -O2 match.c -o match.exe
 */
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
#define U8(s,o,v)  (*(unsigned char*)((char*)(s)+(o))=(unsigned char)(v))

typedef int  (*ppp_t)(int);
typedef int  (*ic_t)(void);
typedef int  (*sc_t)(void*,unsigned*,void*,unsigned*);
typedef int  (*ld_t)(void*,unsigned,void*,unsigned);
typedef unsigned (*cal_len_t)(void);
typedef int  (*chicago_load_t)(void*,unsigned);
typedef int  (*sm_t)(int);
typedef int  (*ppi_t)(void*);
typedef int  (*pp_t)(void*,void*,void*,void*,void*,char,char);
typedef void*(*es_t)(void);
typedef int  (*eai_t)(void*,void*,void*,void*,char,void*);
typedef int  (*egt_t)(void*,void**);
typedef int  (*idt_t)(void*,void*,void*,int*);                         /* identifytemplate(gal,probe,?,&idx) */
typedef int  (*idi_t)(void*,void*,void*,int,int*,int*,void*,int,int,void*,int); /* identifyImageWrapper */
typedef int  (*tps_t)(void*);                                           /* templateGetPackedSize */
typedef int  (*tp_t)(void*,void*);                                      /* templatePack */
#define G(t,n) (t)(void*)GetProcAddress(h,n)

static HMODULE h;
static pp_t  preprocessor;
static es_t  enrolStart;
static eai_t enrolAddImage;
static egt_t enrolGetTemplate;

/* preprocess one frame file into the `out` enhanced-image struct. Returns
 * quality/coverage via out params. */
static void preprocess_one(const char* path, char* out, unsigned char* enh,
                           void* a1, void* a2, char* qc, char* in,
                           unsigned short* raw, int* qual, int* cov)
{
    FILE* f=fopen(path,"rb");
    memset(raw,0,FB);
    if(f){ if(fread(raw,1,FB,f)){} fclose(f); }
    memset(enh,0,IB);
    P(in,0,raw); U16(in,8,W); U16(in,0xa,H); U32(in,0x14,FB); U16(in,0x18,1);
    P(out,0,enh); U16(out,8,W); U16(out,0xa,H); U32(out,0x14,IB);
    preprocessor(in,a1,a2,out,qc,0,0);
    *qual=(unsigned char)out[0x28]; *cov=(unsigned char)out[0x29];
    U8(out,0x0e,8); U8(out,0x0f,1); U16(out,0x18,1);
}

int main(int argc, char** argv)
{
    if (argc < 4){ printf("usage: match.exe <base.raw> <Ngal> <frames...>\n"); return 2; }
    { const char *dll=getenv("ALGO_DLL");
      h = LoadLibraryA(dll && *dll ? dll : "AlgoMilan.dll"); }
    if(!h){ printf("load fail\n"); return 1; }
    { const char* hs=getenv("HOOK_SLEEP"); if(hs){ printf("HOOK_SLEEP %s s (attach BPs now)\n",hs); fflush(stdout); Sleep(atoi(hs)*1000); } }
    const char* base_path = argv[1];
    int ngal = atoi(argv[2]);
    char** frames = &argv[3];
    int nframes = argc - 3;
    printf("Algorithm base=%p  ngal=%d nframes=%d\n",(void*)h,ngal,nframes);

    { const char* pe=getenv("PPP"); int pv=pe?atoi(pe):10;
      ((ppp_t)G(ppp_t,"ppp_param_init"))(pv);
      printf("ppp_param_init(%d)\n",pv); }
    ((ic_t)G(ic_t,"preprocess_init_calidata"))();
    { const char *dll = getenv("ALGO_DLL");
      int chicago = dll && strstr(dll, "AlgoChicago.dll");
      if (chicago) {
          /* Chicago's calibration ABI is not Milan's: the production Engine
           * loads [16-byte sensor id][one 0x224b0-byte blob] from this file
           * via preprocess_load_calidata(blob, size). */
          const char *cal_path = getenv("CHICAGO_CAL");
          FILE *cf = fopen(cal_path ? cal_path :
              "Z:\\mnt\\win3\\ProgramData\\Goodix\\goodix_calib.dat", "rb");
          cal_len_t cal_len = G(cal_len_t,"preprocess_get_calidata_len");
          chicago_load_t chicago_load = G(chicago_load_t,"preprocess_load_calidata");
          unsigned len = cal_len ? cal_len() : 0;
          unsigned char *blob = len ? calloc(1, len) : NULL;
          int lr = -1;
          if (cf && blob) {
              fseek(cf, 16, SEEK_SET);
              if (fread(blob, 1, len, cf) == len) lr = chicago_load(blob, len);
          }
          if (cf) fclose(cf);
          printf("Chicago real calibration len=0x%x load=0x%x\n", len, lr);
          free(blob);
      } else {
          ld_t load=G(ld_t,"preprocess_load_calidata");
          void*c1=calloc(1,0x184ac); void*c2=calloc(1,0xa004); unsigned s1=0x184ac,s2=0xa004;
          ((sc_t)G(sc_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2);
          load(c1,s1,c2,s2);
      }
    }

    /* preprocessor_init: build Kr from the no-finger base (THE key step) */
    unsigned short* basefr=calloc(1,FB);
    { FILE*bf=fopen(base_path,"rb"); if(bf){ if(fread(basefr,1,FB,bf)){} fclose(bf);} }
    char* cfg=calloc(1,0x40); P(cfg,0x18,basefr); U32(cfg,0x24,W); U32(cfg,0x28,H);
    ((ppi_t)G(ppi_t,"preprocessor_init"))(cfg);
    ((sm_t)G(sm_t,"preprocess_set_mode"))(1);

    preprocessor=G(pp_t,"preprocessor");
    enrolStart=G(es_t,"enrolStart");
    enrolAddImage=G(eai_t,"enrolAddImage");
    enrolGetTemplate=G(egt_t,"enrolGetTemplate");
    idt_t identifytemplate=G(idt_t,"identifytemplate");
    idi_t identifyImage=G(idi_t,"identifyImageWrapper");

    /* The EngineAdapter passes a current-feature object, not only the public
     * 0x40-byte image header: identifyImage receives its auxiliary context at
     * current+0x40.  ENGINE_ABI enables that offline ABI reproduction. */
    int engine_abi = getenv("ENGINE_ABI") != NULL;
    unsigned short* raw=calloc(1,FB); unsigned char* enh=calloc(1,IB);
    char* in=calloc(1,SS); char* out=calloc(1,engine_abi ? 0x5000 : SS);
    void* engine_meta = engine_abi ? calloc(1,0x10000) : NULL;
    if (engine_abi) {
        /* Exact installed EngineAdapter call site: mode=current[0x2c0],
         * opaque matcher context=*(void **)(current+0x4cf8),
         * study flag=current[0x431]. */
        P(out,0x4cf8,engine_meta);
        U8(out,0x431,1);
    }
    void* a1=calloc(1,0x8000);
    /* EngineAdapter invokes preprocessor_wrapper with a single current-feature
     * object: its arg3 is current+0x40 and its quality/control block is at
     * current+0x30.  Keep the legacy independent scratch allocation outside
     * ENGINE_ABI so existing experiments retain their exact behavior. */
    void* a2=engine_abi ? out + 0x40 : calloc(1,0x8000);
    char* qc=engine_abi ? out + 0x30 : calloc(1,0x20);
    void* ea2=calloc(1,0x8000); void* ea3=calloc(1,0x8000);
    /* Test whether the enrolment side's metadata is the opaque pointer which
     * EngineAdapter later supplies to identifyImage. */
    char* meta=engine_abi ? engine_meta : calloc(1,0x80);

    /* ---- build gallery template ---- */
    void* ctx=enrolStart();
    for(int i=0;i<ngal;i++){
        int q,c; preprocess_one(frames[i],out,enh,a1,a2,qc,in,raw,&q,&c);
        int r=enrolAddImage(ctx,out,ea2,ea3,0,meta);
        unsigned samples=*(unsigned short*)((char*)ctx+0xa);
        unsigned prog=*(unsigned*)((char*)ctx+0xc);
        printf("[GAL] %-40s q=%2d cov=%2d add=0x%x samples=%u prog=%u%%\n",
               frames[i],q,c,r,samples,prog);
    }
    void* gtpl=NULL; int gr=enrolGetTemplate(ctx,&gtpl);
    unsigned char* inner = gtpl?*(unsigned char**)gtpl:NULL;
    printf("[GAL] enrolGetTemplate ret=0x%x tpl=%p inner=%p count=%u\n",
           gr,gtpl,inner, inner?*(unsigned*)(inner+0x1c):0);
    if (getenv("PACK_TEMPLATE") && gtpl) {
        int packed_size = ((tps_t)G(tps_t,"templateGetPackedSize"))(gtpl);
        unsigned char *packed = packed_size > 0 ? calloc(1, (size_t)packed_size) : NULL;
        int packed_ret = packed ? ((tp_t)G(tp_t,"templatePack"))(gtpl, packed) : -1;
        printf("[GAL] templateGetPackedSize=%d templatePack=0x%x first16=", packed_size, packed_ret);
        for (int j=0; packed && j<16 && j<packed_size; ++j) printf("%02x", packed[j]);
        putchar('\n');
        { const char *dump = getenv("DUMP_PACK");
          if (dump && packed && packed_ret == 0) {
              FILE *df = fopen(dump, "wb");
              if (df) { fwrite(packed, 1, (size_t)packed_size, df); fclose(df); }
              printf("[GAL] packed template written to %s\n", dump);
          } }
        free(packed);
    }
    void* galArray[4]; galArray[0]=gtpl;

    /* ---- probe each remaining frame ---- */
    printf("--- probing %d held-out frames ---\n", nframes-ngal);
    int matches=0, tested=0;
    int template_each = getenv("TEMPLATE_EACH") != NULL;
    for(int i=ngal;i<nframes;i++){
        /* Diagnostic only: distinguish intended rolling preprocessor state
         * from stale scratch bytes in the standalone wrapper reconstruction. */
        if (getenv("CLEAR_PROBE")) {
            memset(out, 0, engine_abi ? 0x5000 : SS);
            memset(a1, 0, 0x8000);
            if (!engine_abi) memset(a2, 0, 0x8000);
            if (!engine_abi) memset(qc, 0, 0x20);
        }
        int q,c; preprocess_one(frames[i],out,enh,a1,a2,qc,in,raw,&q,&c);
        int idx=-999, score=-999;
        if (engine_abi)
            memcpy(engine_meta, raw, FB); /* EngineAdapter's current+0x4cf8 raw resource */
        char* pmeta=calloc(1,engine_abi ? 0x1000 : 0x80);
        void *probe_ctx = engine_abi ? out + 0x40 : NULL;
        int mode = engine_abi ? (unsigned char)out[0x2c0] : 0;
        int study = engine_abi ? (unsigned char)out[0x431] : 0;
        U8(out,0x0e,8); U8(out,0x0f,1);
        if (!getenv("TEMPLATE_ONLY")) {
            int rc = identifyImage(out, probe_ctx, galArray, 1, &idx, &score, pmeta,
                                   0, mode,
                                   engine_abi ? *(void **)(out+0x4cf8) : NULL,
                                   study);
            printf("[PRB] %-40s q=%2d cov=%2d  identifyImage%s rc=0x%x idx=%d score=%d %s\n",
                   frames[i],q,c,engine_abi?"[engine-abi]":"",rc,idx,score,
                   idx>=0?"*** MATCH ***":"");
            tested++; if(idx>=0) matches++;
        }
        if (template_each && identifytemplate) {
            void *pctx = enrolStart(), *ptpl = NULL;
            int tidx = -999, trc;
            if (pctx) {
                int tq, tc;
                preprocess_one(frames[i],out,enh,a1,a2,qc,in,raw,&tq,&tc);
                enrolAddImage(pctx,out,ea2,ea3,0,meta);
                enrolGetTemplate(pctx,&ptpl);
            }
            trc = ptpl ? identifytemplate(gtpl,ptpl,NULL,&tidx) : -1;
            printf("[TPL] %-40s rc=0x%x idx=%d %s\n", frames[i],trc,tidx,
                   tidx>=0?"*** MATCH ***":"no-match");
        }
        free(pmeta);
    }
    printf("\n=== identifyImage: %d/%d probes MATCHED ===\n", matches, tested);

    /* ---- also the template-vs-template path for a probe template ---- */
    if (ngal < nframes && identifytemplate){
        void* pctx=enrolStart();
        for(int i=ngal;i<nframes;i++){
            int q,c; preprocess_one(frames[i],out,enh,a1,a2,qc,in,raw,&q,&c);
            enrolAddImage(pctx,out,ea2,ea3,0,meta);
        }
        void* ptpl=NULL; enrolGetTemplate(pctx,&ptpl);
        int idx=-999; int rc=identifytemplate(gtpl, ptpl, NULL, &idx);
        printf("identifytemplate(gal,probe) rc=0x%x idx=%d %s\n",
               rc,idx, idx>=0?"*** MATCH ***":"no-match");
        int idx2=-999; int rc2=identifytemplate(gtpl, gtpl, NULL, &idx2);
        printf("identifytemplate(gal,gal)   rc=0x%x idx=%d %s\n",
               rc2,idx2, idx2>=0?"*** MATCH ***":"no-match");
    }
    printf("done\n");
    return 0;
}
