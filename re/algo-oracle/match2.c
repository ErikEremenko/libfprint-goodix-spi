// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* AlgoMilan MATCH harness v2 — test the serialize/reload finalize hypothesis.
 * Builds gallery + probe templates (dumpfeat preprocessing), then compares:
 *   A) identifytemplate(gal_raw,  probe_raw)      baseline
 *   B) identifytemplate(gal_packed_reloaded, probe_raw)
 *   C) identifytemplate(gal_reloaded, probe_reloaded)
 *   + templateStudy(&count) before each.
 *   match2.exe <base.raw> <Ngal> <frames...>
 * Build: x86_64-w64-mingw32-gcc -O2 match2.c -o match2.exe */
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
typedef int (*ppp_t)(int); typedef int (*ic_t)(void);
typedef int (*sc_t)(void*,unsigned*,void*,unsigned*); typedef int (*ld_t)(void*,unsigned,void*,unsigned);
typedef int (*sm_t)(int); typedef int (*ppi_t)(void*);
typedef int (*pp_t)(void*,void*,void*,void*,void*,char,char);
typedef void*(*es_t)(void); typedef int (*eai_t)(void*,void*,void*,void*,char,void*);
typedef int (*egt_t)(void*,void**);
typedef int (*idt_t)(void*,void*,void*,int*);
typedef int (*gps_t)(void*);              /* templateGetPackedSize(tpl) -> size */
typedef int (*pk_t)(void*,void*);         /* templatePack(tpl,buf) */
typedef int (*upk_t)(void*,int,void*,void**); /* templateUnPack(buf,size,param,&out) */
typedef int (*stu_t)(int*);               /* templateStudy(&count) over global DB */
#define G(t,n) (t)(void*)GetProcAddress(h,n)
static HMODULE h;
static pp_t preprocessor; static es_t enrolStart; static eai_t enrolAddImage; static egt_t enrolGetTemplate;

static void ppone(const char* path,char*out,unsigned char*enh,void*a1,void*a2,char*qc,char*in,unsigned short*raw,int*q,int*c){
    FILE*f=fopen(path,"rb"); memset(raw,0,FB); if(f){if(fread(raw,1,FB,f)){}fclose(f);} memset(enh,0,IB);
    P(in,0,raw);U16(in,8,W);U16(in,0xa,H);U32(in,0x14,FB);U16(in,0x18,1);
    P(out,0,enh);U16(out,8,W);U16(out,0xa,H);U32(out,0x14,IB);
    preprocessor(in,a1,a2,out,qc,0,0);
    *q=(unsigned char)out[0x28]; *c=(unsigned char)out[0x29];
    U8(out,0x0e,8);U8(out,0x0f,1);U16(out,0x18,1);
}
static void* build(char**frames,int n,const char*base_path,const char*tag,
                   void*a1,void*a2,void*ea2,void*ea3,char*meta,
                   unsigned short*raw,unsigned char*enh,char*in,char*out,char*qc){
    void*ctx=enrolStart();
    for(int i=0;i<n;i++){int q,c; ppone(frames[i],out,enh,a1,a2,qc,in,raw,&q,&c);
        enrolAddImage(ctx,out,ea2,ea3,0,meta);}
    void*tpl=NULL; enrolGetTemplate(ctx,&tpl);
    unsigned char* inner=tpl?*(unsigned char**)tpl:NULL;
    printf("[%s] tpl=%p inner=%p count=%u [0x14c]=%u\n",tag,tpl,inner,
           inner?*(unsigned*)(inner+0x1c):0, inner?*(unsigned*)(inner+0x14c):0);
    return tpl;
}
int main(int argc,char**argv){
    if(argc<4){printf("usage: match2 <base> <Ngal> <frames...>\n");return 2;}
    h=LoadLibraryA("AlgoMilan.dll");
    const char* base_path=argv[1]; int ngal=atoi(argv[2]);
    char** frames=&argv[3]; int nf=argc-3;
    ((ppp_t)G(ppp_t,"ppp_param_init"))(10);
    ((ic_t)G(ic_t,"preprocess_init_calidata"))();
    ld_t load=G(ld_t,"preprocess_load_calidata");
    void*c1=calloc(1,0x184ac);void*c2=calloc(1,0xa004);unsigned s1=0x184ac,s2=0xa004;
    ((sc_t)G(sc_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2); load(c1,s1,c2,s2);
    unsigned short* basefr=calloc(1,FB);
    { FILE*bf=fopen(base_path,"rb"); if(bf){if(fread(basefr,1,FB,bf)){}fclose(bf);} }
    char* cfg=calloc(1,0x40); P(cfg,0x18,basefr); U32(cfg,0x24,W); U32(cfg,0x28,H);
    ((ppi_t)G(ppi_t,"preprocessor_init"))(cfg);
    ((sm_t)G(sm_t,"preprocess_set_mode"))(1);
    preprocessor=G(pp_t,"preprocessor");
    enrolStart=G(es_t,"enrolStart"); enrolAddImage=G(eai_t,"enrolAddImage"); enrolGetTemplate=G(egt_t,"enrolGetTemplate");
    idt_t identifytemplate=G(idt_t,"identifytemplate");
    gps_t getsz=G(gps_t,"templateGetPackedSize");
    pk_t  pack =G(pk_t,"templatePack");
    upk_t unpk =G(upk_t,"templateUnPack");
    stu_t study=G(stu_t,"templateStudy");
    printf("getsz=%p pack=%p unpk=%p study=%p\n",getsz,pack,unpk,study);

    unsigned short* raw=calloc(1,FB); unsigned char* enh=calloc(1,IB);
    char* in=calloc(1,SS); char* out=calloc(1,SS); char* qc=calloc(1,0x40);
    void* a1=calloc(1,0x8000); void* a2=calloc(1,0x8000);
    void* ea2=calloc(1,0x8000); void* ea3=calloc(1,0x8000); char* meta=calloc(1,0x80);

    void* gal=build(frames,ngal,base_path,"GAL",a1,a2,ea2,ea3,meta,raw,enh,in,out,qc);
    void* prb=build(frames+ngal,nf-ngal,base_path,"PRB",a1,a2,ea2,ea3,meta,raw,enh,in,out,qc);

    /* templateStudy over global DB */
    if(study){ int cnt=-999; int rc=study(&cnt); printf("templateStudy rc=0x%x count=%d\n",rc,cnt); }

    int idx;
    if(gal&&prb){ idx=-999; int rc=identifytemplate(gal,prb,NULL,&idx);
        printf("A) raw gal vs raw prb        rc=0x%x idx=%d %s\n",rc,idx,idx>=0?"*** MATCH ***":"no-match"); }

    /* B) pack+unpack the gallery, then match */
    void* gal2=NULL;
    if(gal&&getsz&&pack&&unpk){
        int sz=getsz(gal); printf("gal packed size=%d\n",sz);
        if(sz>0){ void*buf=calloc(1,sz+256); int pr=pack(gal,buf);
            int ur=unpk(buf,sz,NULL,&gal2);
            printf("templatePack rc=0x%x  templateUnPack rc=0x%x gal2=%p\n",pr,ur,gal2);
            if(gal2){ unsigned char* i2=*(unsigned char**)gal2;
                printf("   gal2 inner=%p count=%u [0x14c]=%u\n",i2,i2?*(unsigned*)(i2+0x1c):0,i2?*(unsigned*)(i2+0x14c):0);
                if(study){int cnt=-999;study(&cnt);}
                idx=-999; int rc=identifytemplate(gal2,prb,NULL,&idx);
                printf("B) reloaded gal vs raw prb   rc=0x%x idx=%d %s\n",rc,idx,idx>=0?"*** MATCH ***":"no-match");
            }
        }
    }
    /* C) pack+unpack both */
    void* prb2=NULL;
    if(prb&&getsz&&pack&&unpk){
        int sz=getsz(prb);
        if(sz>0){ void*buf=calloc(1,sz+256); pack(prb,buf); unpk(buf,sz,NULL,&prb2); }
    }
    if(gal2&&prb2){ if(study){int cnt=-999;study(&cnt);}
        idx=-999; int rc=identifytemplate(gal2,prb2,NULL,&idx);
        printf("C) reloaded gal vs reloaded prb rc=0x%x idx=%d %s\n",rc,idx,idx>=0?"*** MATCH ***":"no-match"); }
    /* self sanity on reloaded */
    if(gal2){ idx=-999; int rc=identifytemplate(gal2,gal2,NULL,&idx);
        printf("   self reloaded gal/gal       rc=0x%x idx=%d %s\n",rc,idx,idx>=0?"MATCH":"no-match"); }
    printf("done\n");
    return 0;
}
