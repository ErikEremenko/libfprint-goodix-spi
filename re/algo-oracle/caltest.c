// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Probe how the real calidata should be loaded: try param 10 vs 12, query
 * get_calidata_len, and attempt preprocess_load_calidata on the real blobs. */
#include <windows.h>
#include <stdio.h>
typedef int (*ppp_t)(int); typedef int (*ic_t)(void);
typedef void (*gl_t)(unsigned*,unsigned*);
typedef int (*ld_t)(void*,unsigned,void*,unsigned);
typedef int (*gv_t)(char*);
#define GET(t,n) (t)(void*)GetProcAddress(h,n)
static HMODULE h;
static void* rd(const char*p, unsigned* n){ FILE*f=fopen(p,"rb"); if(!f){*n=0;return 0;}
    fseek(f,0,SEEK_END); long s=ftell(f); fseek(f,0,SEEK_SET);
    void*b=malloc(s); fread(b,1,s,f); fclose(f); *n=(unsigned)s; return b; }
int main(int argc,char**argv){
    h=LoadLibraryA("AlgoMilan.dll");
    char ver[128]={0}; ((gv_t)GET(gv_t,"getAlgorithmVersion"))(ver);
    printf("version: %s\n", ver);
    /* full calib file: 16-byte sensorid header + b1(0x184ac) + b2(0xa004) */
    unsigned cn; unsigned char* cal=rd("goodix_calib.dat",&cn);
    printf("goodix_calib.dat = %u bytes\n", cn);
    ld_t load=GET(ld_t,"preprocess_load_calidata");
    for(int p=10;p<=12;p+=2){
        ((ppp_t)GET(ppp_t,"ppp_param_init"))(p);
        ((ic_t)GET(ic_t,"preprocess_init_calidata"))();
        unsigned a=0,b=0; ((gl_t)GET(gl_t,"preprocess_get_calidata_len"))(&a,&b);
        printf("\n[param %d] get_calidata_len -> s1=%u(0x%x) s2=%u(0x%x)\n",p,a,a,b,b);
        if(!cal) continue;
        /* correct layout: [16B sensorid][b2 = 0xa004][b1 = 0x184ac] */
        void* b2r=cal+16;            /* 0xa004 blob */
        void* b1r=cal+16+0xa004;     /* 0x184ac blob (version str @ +0x1848c) */
        int r=load(b1r,a,b2r,b);
        printf("  b2-first split: load_calidata(b1@0x%x s1=%u, b2@0x10 s2=%u) ret=0x%x\n",
               16+0xa004, a, b, r);
        ((ic_t)GET(ic_t,"preprocess_init_calidata"))(); /* reset */
    }
    return 0;
}
