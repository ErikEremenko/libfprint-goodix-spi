// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * AlgoMilan.dll oracle loader — runs under Wine.
 *
 * Milestone 1: prove the DLL loads under Wine and the exported entry points
 * resolve + run, by calling getAlgorithmVersion (should print "Milan_v_3.00.20").
 * Later this grows into the preprocessing oracle (ppp_param_init ->
 * preprocess_load_calidata -> preprocessor on a real frame, dumping buffers).
 *
 * Build (needs mingw64-gcc):
 *   x86_64-w64-mingw32-gcc -O2 loader.c -o loader.exe
 * Run (from the dir containing AlgoMilan.dll):
 *   WINEDEBUG=-all wine loader.exe
 */
#include <windows.h>
#include <stdio.h>

typedef int  (*getver_t)(char *);
typedef int  (*ppp_param_init_t)(int index);
typedef int  (*init_calidata_t)(void);
typedef void (*get_calidata_len_t)(unsigned *, unsigned *);
typedef int  (*save_calidata_t)(void *, unsigned *, void *, unsigned *);
typedef int  (*load_calidata_t)(void *, unsigned, void *, unsigned);
typedef int  (*set_mode_t)(int);
/* preprocessor(input, arg1, arg2, output, qc, arg5, mode) -> int GF error */
typedef int  (*preprocessor_t)(void *, void *, void *, void *, void *, char, char);

#define GET(t, n) (t)(void *)GetProcAddress(h, n)

/* Preset 10 geometry (matches the 64x80 sensor). */
#define W 64
#define H 80
#define FRAME_BYTES (2 * W * H)   /* 16-bpp input: 10240 */
#define IMG_BYTES   (W * H)       /* 8-bit output:  5120  */
#define STRUCT_SZ   0x40          /* generous image-struct size */

/* Field pokes at the offsets recovered from enrolAddImage. */
#define PUT_PTR(s, off, v) (*(void **)((char *)(s) + (off)) = (void *)(v))
#define PUT_U16(s, off, v) (*(unsigned short *)((char *)(s) + (off)) = (unsigned short)(v))
#define PUT_U32(s, off, v) (*(unsigned *)((char *)(s) + (off)) = (unsigned)(v))

int main(void)
{
    HMODULE h = LoadLibraryA("AlgoMilan.dll");
    if (!h) {
        printf("LoadLibrary(AlgoMilan.dll) failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    printf("AlgoMilan.dll loaded at %p\n", (void *)h);

    getver_t getver = GET(getver_t, "getAlgorithmVersion");
    if (getver) {
        char ver[256]; memset(ver, 0, sizeof(ver));
        int r = getver(ver);
        printf("getAlgorithmVersion() ret=0x%x version=\"%s\"\n", r, ver);
    }

    /* Calibration-data sizes. */
    get_calidata_len_t getlen = GET(get_calidata_len_t, "preprocess_get_calidata_len");
    if (getlen) {
        unsigned a = 0, b = 0;
        getlen(&a, &b);
        printf("preprocess_get_calidata_len -> %u (0x%x), %u (0x%x)\n", a, a, b, b);
    }

    /* Init sequence: preset 10 == 64x80 sensor geometry, then default cal. */
    ppp_param_init_t ppp = GET(ppp_param_init_t, "ppp_param_init");
    if (ppp) printf("ppp_param_init(10) ret=0x%x\n", ppp(10));

    init_calidata_t initcal = GET(init_calidata_t, "preprocess_init_calidata");
    if (initcal) printf("preprocess_init_calidata() ret=0x%x\n", initcal());

    /* CALIBRATION POKE: overwrite the per-pixel offset map (rbx+0x9924, rbx =
     * cal-array base @RVA 0xc14d0) with a uniform baseline before serialization,
     * so the calibration subtracts the ~1500 DC.  argv[2] = offset value. */
    unsigned short *offmap = (unsigned short *)((char *)h + 0xc14d0 + 0x9924);
    const char *off_arg = (__argc > 2) ? __argv[2] : "0";
    unsigned short gain_val = (__argc > 3) ? (unsigned short)atoi(__argv[3]) : 0;
    FILE *of = fopen(off_arg, "rb");   /* per-pixel baseline .raw file? */
    if (of) {
        /* offset_map[i] = baseline[i] + C  (calibration math: calibrated=offset-raw). */
        unsigned short base[W*H];
        size_t got = fread(base, 2, W*H, of); fclose(of);
        unsigned C = (__argc > 4) ? (unsigned)atoi(__argv[4]) : 2200;
        for (int i = 0; i < W*H; i++) offmap[i] = (unsigned short)(base[i] + C);
        printf("poked per-pixel offset = baseline(%s)+%u (%zu px)\n", off_arg, C, got);
    } else {
        unsigned short off_val = (unsigned short)atoi(off_arg);
        if (off_val) { for (int i = 0; i < 8192; i++) offmap[i] = off_val;
            printf("poked uniform offset = %u\n", off_val); }
    }
    if (gain_val) {
        unsigned short *gainmap = (unsigned short *)((char *)h + 0xc14d0 + 4);
        for (int i = 0; i < 8192; i++) gainmap[i] = gain_val;
        printf("poked gain map = %u\n", gain_val);
    }

    /* Serialize the default calibration into valid blobs (version + CRCs), then
     * load them back — this is what sets the "calibration loaded" flag that
     * preprocessor gates on.  cal buffer sizes from preprocess_get_calidata_len. */
    void *cal1 = calloc(1, 0x184ac);
    void *cal2 = calloc(1, 0xa004);
    unsigned s1 = 0x184ac, s2 = 0xa004;
    save_calidata_t save = GET(save_calidata_t, "preprocess_save_calidata");
    if (save) printf("preprocess_save_calidata() ret=0x%x (s1=%u s2=%u)\n",
                     save(cal1, &s1, cal2, &s2), s1, s2);
    load_calidata_t load = GET(load_calidata_t, "preprocess_load_calidata");
    if (load) printf("preprocess_load_calidata() ret=0x%x\n", load(cal1, s1, cal2, s2));

    /* preprocess_set_mode writes the "mode" global that preprocessor gates on. */
    set_mode_t setmode = GET(set_mode_t, "preprocess_set_mode");
    if (setmode) printf("preprocess_set_mode(1) ret=0x%x\n", setmode(1));

    /* Build input (16-bpp frame) + output (8-bit image) structs. */
    unsigned short *frame = (unsigned short *)calloc(1, FRAME_BYTES);
    unsigned char  *outbuf = (unsigned char *)calloc(1, IMG_BYTES);
    char *in  = (char *)calloc(1, STRUCT_SZ);
    char *out = (char *)calloc(1, STRUCT_SZ);
    char *qc  = (char *)calloc(1, 0x20);

    /* Load a real captured frame (10240 bytes, 16-bpp LE) from argv[1], else a
     * synthetic gradient. */
    const char *frame_path = (__argc > 1) ? __argv[1] : "frame.raw";
    FILE *ff = fopen(frame_path, "rb");
    if (ff) {
        size_t got = fread(frame, 1, FRAME_BYTES, ff);
        fclose(ff);
        printf("loaded frame %s (%zu bytes)\n", frame_path, got);
    } else {
        printf("no frame file (%s); using synthetic gradient\n", frame_path);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                frame[y*W + x] = (unsigned short)((x * 4095) / (W - 1));
    }

    PUT_PTR(in,  0x00, frame);
    PUT_U16(in,  0x08, W);
    PUT_U16(in,  0x0a, H);
    PUT_U32(in,  0x14, FRAME_BYTES);
    PUT_U16(in,  0x18, 1);            /* frame_count */

    PUT_PTR(out, 0x00, outbuf);
    PUT_U16(out, 0x08, W);
    PUT_U16(out, 0x0a, H);
    PUT_U32(out, 0x14, IMG_BYTES);

    printf("flags: params=%d mode=%d\n",
           *(int *)((char *)h + 0xc14cc), *(int *)((char *)h + 0xf195c));

    /* arg1/arg2 are required buffers the engine dereferences (NULL -> crash).
     * Give them generous zeroed scratch for now. */
    void *arg1 = calloc(1, 0x8000);
    void *arg2 = calloc(1, 0x8000);

    preprocessor_t pp = GET(preprocessor_t, "preprocessor");
    printf("calling preprocessor(in=%p a1=%p a2=%p out=%p) ...\n",
           (void*)in, arg1, arg2, (void*)out);
    fflush(stdout);
    int pr = pp(in, arg1, arg2, out, qc, 0, 0);
    printf("preprocessor ret=0x%x\n", pr);

    /* Stats for out + the arg1/arg2 scratch (where did the enhanced image go?). */
    #define STATS(name, p, len) do { \
        unsigned _mn=255,_mx=0; unsigned long _s=0; int _nz=0; \
        const unsigned char *_b=(const unsigned char*)(p); \
        for (int _i=0;_i<(len);_i++){unsigned _v=_b[_i]; if(_v<_mn)_mn=_v; if(_v>_mx)_mx=_v; _s+=_v; if(_v)_nz++;} \
        printf("  %-6s min=%u max=%u mean=%lu nonzero=%d/%d\n", name,_mn,_mx,_s/(len),_nz,(len)); \
    } while(0)
    printf("buffers after preprocessor:\n");
    STATS("out",  outbuf, IMG_BYTES);
    STATS("arg1", arg1,   0x8000);
    STATS("arg2", arg2,   0x8000);
    printf("qc bytes: "); for (int i=0;i<16;i++) printf("%02x ",(unsigned char)((char*)qc)[i]); printf("\n");
    printf("out struct 0x08..0x30: "); for (int i=8;i<0x30;i++) printf("%02x ",(unsigned char)out[i]); printf("\n");
    unsigned mn=255,mx=0; unsigned long sum=0;
    for (int i=0;i<IMG_BYTES;i++){unsigned v=outbuf[i]; if(v<mn)mn=v; if(v>mx)mx=v; sum+=v;}
    printf("output: min=%u max=%u mean=%lu  quality=%u coverage=%u\n",
           mn, mx, sum / IMG_BYTES, (unsigned char)out[0x29], (unsigned char)out[0x28]);
    FILE *f = fopen("preproc_out.raw", "wb");
    if (f) { fwrite(outbuf, 1, IMG_BYTES, f); fclose(f); printf("wrote preproc_out.raw (%d bytes)\n", IMG_BYTES); }

    printf("done\n");
    return 0;
}
