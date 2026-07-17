// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* dump my DLL's own save_calidata blobs so we can diff vs the real ones and
 * find the version/header field that makes load_calidata reject the real blob. */
#include <windows.h>
#include <stdio.h>
typedef int (*ppp_t)(int); typedef int (*ic_t)(void);
typedef int (*sc_t)(void*,unsigned*,void*,unsigned*);
#define GET(t,n) (t)(void*)GetProcAddress(h,n)
int main(void){
    HMODULE h=LoadLibraryA("AlgoMilan.dll");
    ((ppp_t)GET(ppp_t,"ppp_param_init"))(10);
    ((ic_t)GET(ic_t,"preprocess_init_calidata"))();
    void*c1=calloc(1,0x184ac);void*c2=calloc(1,0xa004);unsigned s1=0x184ac,s2=0xa004;
    int r=((sc_t)GET(sc_t,"preprocess_save_calidata"))(c1,&s1,c2,&s2);
    printf("save ret=0x%x s1=%u s2=%u\n",r,s1,s2);
    FILE*f=fopen("mine_b1.bin","wb");fwrite(c1,1,0x184ac,f);fclose(f);
    f=fopen("mine_b2.bin","wb");fwrite(c2,1,0xa004,f);fclose(f);
    printf("wrote mine_b1.bin mine_b2.bin\n");
    return 0;
}
