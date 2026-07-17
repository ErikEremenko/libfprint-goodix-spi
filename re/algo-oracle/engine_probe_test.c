// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * Minimal WBDI EngineAdapter interface probe.  This is RE tooling only: it
 * loads the exact driver-store EngineAdapter under Wine and prints the
 * published WINBIO_ENGINE_INTERFACE table before any callbacks are invoked.
 *
 * Build: x86_64-w64-mingw32-gcc -O2 engine_probe.c -o engine_probe.exe
 * Run from the DriverStore directory so its sibling AlgoChicago.dll is used:
 *   WINEDEBUG=-all wine Z:\\path\\to\\engine_probe.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stddef.h>

typedef struct {
    unsigned short major, minor;
} adapter_version_t;

typedef struct {
    adapter_version_t Version;
    unsigned Type;
    size_t Size;
    GUID AdapterId;
    void *Attach;
    void *Detach;
    void *ClearContext;
    void *QueryPreferredFormat;
    void *QueryIndexVectorSize;
    void *QueryHashAlgorithms;
    void *SetHashAlgorithm;
    void *QuerySampleHint;
    void *AcceptSampleData;
    void *ExportEngineData;
    void *VerifyFeatureSet;
    void *IdentifyFeatureSet;
    void *CreateEnrollment;
    void *UpdateEnrollment;
    void *GetEnrollmentStatus;
    void *GetEnrollmentHash;
    void *CheckForDuplicate;
    void *CommitEnrollment;
    void *DiscardEnrollment;
    void *ControlUnit;
    void *ControlUnitPrivileged;
    void *NotifyPowerChange;
    void *Reserved1;
    void *PipelineInit;
    void *PipelineCleanup;
    void *Activate;
    void *Deactivate;
    void *QueryExtendedInfo;
    void *IdentifyAll;
    void *SetEnrollmentSelector;
    void *SetEnrollmentParameters;
    void *QueryExtendedEnrollmentStatus;
    void *RefreshCache;
    void *SelectCalibrationFormat;
    void *QueryCalibrationData;
    void *SetAccountPolicy;
    void *CreateKey;
    void *IdentifyFeatureSetSecure;
    void *AcceptPrivateSensorTypeInfo;
    void *CreateEnrollmentAuthenticated;
    void *IdentifyFeatureSetAuthenticated;
} engine_interface_t;

typedef HRESULT (WINAPI *query_t)(engine_interface_t **);
typedef BOOL (WINAPI *device_io_t)(HANDLE, DWORD, LPVOID, DWORD,
                                  LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef HMODULE (WINAPI *load_library_w_t)(LPCWSTR);
typedef HMODULE (WINAPI *load_library_ex_w_t)(LPCWSTR, HANDLE, DWORD);
typedef HANDLE (WINAPI *create_file_w_t)(LPCWSTR, DWORD, DWORD,
                                         LPSECURITY_ATTRIBUTES, DWORD,
                                         DWORD, HANDLE);
static load_library_w_t real_load_library_w;
static load_library_ex_w_t real_load_library_ex_w;
static create_file_w_t real_create_file_w;
typedef struct { unsigned char *address, original; } reject_bp_t;
static reject_bp_t reject_bps[8];
static unsigned reject_bp_count;

/* One-shot tracepoints inside the real EngineAdapter enrollment merge.  They
 * observe the Chicago add-image result and the adapter's subsequent spatial
 * policy without changing either result.  The tracepoints are re-armed before
 * every UpdateEnrollment call because each INT3 is restored on first hit. */
typedef struct { unsigned char *address, original; const char *label; } enroll_policy_bp_t;
static enroll_policy_bp_t enroll_policy_bps[3];
static unsigned enroll_policy_bp_count;
static int enroll_policy_veh_installed;
static unsigned enroll_policy_attempt;

static unsigned long long fnv1a64(const unsigned char *data, size_t size)
{
    unsigned long long hash = 1469598103934665603ULL;
    size_t i;
    for (i = 0; data && i < size; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void fp_state(unsigned *mxcsr, unsigned short *x87cw)
{
    __asm__ volatile ("stmxcsr %0" : "=m" (*mxcsr));
    __asm__ volatile ("fnstcw %0" : "=m" (*x87cw));
}

static void dump_bytes(const char *path, const void *data, size_t size)
{
    FILE *f;
    if (!path || !*path || !data) return;
    f = fopen(path, "wb");
    if (!f) return;
    fwrite(data, 1, size, f);
    fclose(f);
}

static LONG CALLBACK enroll_policy_veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *ctx = ep->ContextRecord;
    unsigned char *hit, *previous, *module;
    unsigned i;

    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
        return EXCEPTION_CONTINUE_SEARCH;
    hit = (unsigned char *)(ULONG_PTR)ctx->Rip;
    previous = hit - 1;
    module = (unsigned char *)GetModuleHandleA("EngineAdapter.dll");
    for (i = 0; i < enroll_policy_bp_count; i++) {
        enroll_policy_bp_t *bp = &enroll_policy_bps[i];
        DWORD old;
        unsigned char *feature, *result_holder, *result, *config;
        unsigned *reject;

        if (bp->address != hit && bp->address != previous) continue;
        if (bp->address == previous) hit = previous;
        if (!strcmp(bp->label, "pre-add")) {
            const unsigned char *algo =
                (const unsigned char *)GetModuleHandleA("AlgoChicago.dll");
            const unsigned char *feature = (const unsigned char *)(ULONG_PTR)ctx->Rdx;
            printf("ENROLL_POLICY[%u] pre-add rva=0x%tx feature=%p "
                   "scratch=%p raw=%p mode=%u metadata=%p\n",
                   enroll_policy_attempt, (ptrdiff_t)(hit - module), feature,
                   (void *)(ULONG_PTR)ctx->R8, (void *)(ULONG_PTR)ctx->R9,
                   *(const unsigned char *)(ULONG_PTR)(ctx->Rsp + 0x20),
                   *(void **)(ULONG_PTR)(ctx->Rsp + 0x28));
            if (feature)
                printf("  header dims=%u/%u bits=%u channels=%u frames=%u "
                       "size=%u q/c=%u/%u state=%u/%u\n",
                       *(const unsigned short *)(feature + 8),
                       *(const unsigned short *)(feature + 0xa),
                       feature[0xe], feature[0xf],
                       *(const unsigned short *)(feature + 0x18),
                       *(const unsigned *)(feature + 0x14),
                       feature[0x28], feature[0x29],
                       *(const unsigned *)(feature + 0x30),
                       *(const unsigned *)(feature + 0x34));
            if (enroll_policy_attempt == 1 && algo)
                dump_bytes(getenv("CHICAGO_DATA_OUT"), algo + 0x8b000, 0xb600);

            VirtualProtect(hit, 1, PAGE_EXECUTE_READWRITE, &old);
            *hit = bp->original;
            VirtualProtect(hit, 1, old, &old);
            ctx->Rip = (DWORD64)(ULONG_PTR)hit;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        feature = *(unsigned char **)(ULONG_PTR)(ctx->Rsp + 0x130);
        result_holder = *(unsigned char **)(ULONG_PTR)(ctx->Rsp + 0x138);
        result = result_holder ? *(unsigned char **)(result_holder + 8) : NULL;
        config = *(unsigned char **)(ULONG_PTR)(ctx->Rsp + 0xc0);
        reject = *(unsigned **)(ULONG_PTR)(ctx->Rsp + 0x140);

        printf("ENROLL_POLICY[%u] %s rva=0x%tx algo=0x%lx "
               "quality=%d coverage=%d overlay=%d preoverlay=%d "
               "scratch=%016llx enhanced=%016llx\n",
               enroll_policy_attempt, bp->label,
               (ptrdiff_t)(hit - module), (unsigned long)ctx->Rax,
               feature ? *(int *)(feature + 0x30) : -1,
               feature ? *(int *)(feature + 0x34) : -1,
               result ? *(int *)(result + 0x10) : -1,
               result ? *(int *)(result + 0x14) : -1,
               feature ? fnv1a64(feature + 0x40, 0x4cb8) : 0ULL,
               feature ? fnv1a64(*(const unsigned char * const *)feature,
                                  80 * 64) : 0ULL);
        {
            const unsigned char *algo =
                (const unsigned char *)GetModuleHandleA("AlgoChicago.dll");
            const void *start_target =
                *(const void * const *)(module + 0x140460);
            const void *add_target =
                *(const void * const *)(module + 0x140470);
            printf("  enrol globals=%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                   algo ? *(const int *)(algo + 0x9c590) : -1,
                   algo ? *(const int *)(algo + 0x9c594) : -1,
                   algo ? *(const int *)(algo + 0x9c598) : -1,
                   algo ? *(const int *)(algo + 0x9c59c) : -1,
                   algo ? *(const int *)(algo + 0x9c5a0) : -1,
                   algo ? *(const int *)(algo + 0x9c5a4) : -1,
                   algo ? *(const int *)(algo + 0x9c5a8) : -1,
                   algo ? *(const int *)(algo + 0x9c5ac) : -1,
                   algo ? *(const int *)(algo + 0x9c5b0) : -1);
            printf("  start target=%p algo-rva=0x%tx\n", start_target,
                   algo && start_target ?
                     (const unsigned char *)start_target - algo : 0);
            printf("  add target=%p algo-rva=0x%tx\n", add_target,
                   algo && add_target ?
                     (const unsigned char *)add_target - algo : 0);
        }
        if (result) {
            unsigned mxcsr;
            unsigned short x87cw;
            fp_state(&mxcsr, &x87cw);
            printf("  fp mxcsr=0x%08x x87cw=0x%04x\n", mxcsr, x87cw);
            const unsigned char *holder = *(const unsigned char * const *)result;
            const unsigned char *handle = holder ?
                *(const unsigned char * const *)holder : NULL;
            const unsigned char *inner = handle ?
                *(const unsigned char * const *)handle : NULL;
            printf("  inner type=%u dims=%u/%u state=%u/%u/%u/%u count=%u "
                   "capacity=%u transforms=%u\n",
                   inner ? *(const unsigned *)(inner + 0x08) : 0,
                   inner ? *(const unsigned *)(inner + 0x0c) : 0,
                   inner ? *(const unsigned *)(inner + 0x10) : 0,
                   inner ? *(const unsigned *)(inner + 0x14) : 0,
                   inner ? *(const unsigned *)(inner + 0x18) : 0,
                   inner ? *(const unsigned *)(inner + 0x1c) : 0,
                   inner ? *(const unsigned *)(inner + 0x20) : 0,
                   inner ? *(const unsigned *)(inner + 0x24) : 0,
                   inner ? *(const unsigned *)(inner + 0x28) : 0,
                   inner ? *(const unsigned *)(inner + 0x2c) : 0);
            if (inner && *(const unsigned *)(inner + 0x24)) {
                const unsigned count = *(const unsigned *)(inner + 0x24);
                const unsigned char *sub =
                    *(const unsigned char * const *)(inner + 0x30 +
                                                     (count - 1) * 8);
                printf("  last-sub records=%u state100=%u state104=%u "
                       "active=%u quality=%u coverage=%u state114=%u\n",
                       sub ? *(const unsigned *)(sub + 0xf0) : 0,
                       sub ? *(const unsigned *)(sub + 0x100) : 0,
                       sub ? *(const unsigned *)(sub + 0x104) : 0,
                       sub ? *(const unsigned *)(sub + 0x108) : 0,
                       sub ? *(const unsigned *)(sub + 0x10c) : 0,
                       sub ? *(const unsigned *)(sub + 0x110) : 0,
                       sub ? *(const unsigned *)(sub + 0x114) : 0);
            }
        }
        if (config) {
            printf("  config overlay=%u preoverlay=%u template=%u mode=%u max_tip=%u "
                   "start=%u end=%u step=%u choose_less=%u del=%u/%u "
                   "directions=%u,%u,%u,%u,%u,%u\n",
                   config[0x42e], config[0x42f], config[0x429], config[0x431],
                   *(unsigned *)(config + 0x45c),
                   *(unsigned *)(config + 0x460),
                   *(unsigned *)(config + 0x464),
                   *(unsigned *)(config + 0x468), config[0x4a5],
                   *(unsigned *)(config + 0x4b0),
                   *(unsigned *)(config + 0x4b4),
                   config[0x46c], config[0x46d], config[0x46e],
                   config[0x46f], config[0x470], config[0x471]);
        }
        printf("  state last=%u cont=%u tip_index=%u previous_tip=%u tipped=%u "
               "touched=%u enrolled=%u used=%u hr=0x%08x reject=%u\n",
               module[0x127928], module[0x127929], module[0x12792a],
               module[0x12792b], module[0x12792c],
               *(unsigned *)(module + 0x127930),
               *(unsigned *)(module + 0x127934),
               *(unsigned *)(module + 0x127938),
               *(unsigned *)(ULONG_PTR)(ctx->Rsp + 0xc8),
               reject ? *reject : 0);

        VirtualProtect(hit, 1, PAGE_EXECUTE_READWRITE, &old);
        *hit = bp->original;
        VirtualProtect(hit, 1, old, &old);
        ctx->Rip = (DWORD64)(ULONG_PTR)hit;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void arm_enrollment_policy_trace(HMODULE module, unsigned attempt)
{
    static const struct { unsigned rva; const char *label; } sites[] = {
        { 0x44a10, "pre-add" },
        { 0x44a16, "post-add" },
        { 0x451e4, "policy-exit" },
    };
    unsigned i;

    enroll_policy_attempt = attempt;
    if (!enroll_policy_veh_installed) {
        AddVectoredExceptionHandler(1, enroll_policy_veh);
        enroll_policy_veh_installed = 1;
    }
    if (!enroll_policy_bp_count) {
        for (i = 0; i < sizeof(sites) / sizeof(sites[0]); i++) {
            enroll_policy_bps[i].address = (unsigned char *)module + sites[i].rva;
            enroll_policy_bps[i].original = *enroll_policy_bps[i].address;
            enroll_policy_bps[i].label = sites[i].label;
        }
        enroll_policy_bp_count = sizeof(sites) / sizeof(sites[0]);
    }
    for (i = 0; i < enroll_policy_bp_count; i++) {
        DWORD old;
        unsigned char *p = enroll_policy_bps[i].address;
        if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) continue;
        *p = 0xcc;
        VirtualProtect(p, 1, old, &old);
    }
}

/* Unit OTP contains a stable sensor serial and factory calibration.  Keep it
 * in a private ignored fixture instead of publishing it with the harness. */
static unsigned char otp[64];
static const char *fake_sensor_base_raw;
static const char *fake_retry_raw;

static int load_otp_fixture(void)
{
    const char *path = getenv("ENGINE_OTP_FILE");
    FILE *f;
    size_t n;
    int extra;

    if (!path || !*path) {
        fprintf(stderr,
                "ENGINE_OTP_FILE must name a private 64-byte OTP fixture\n");
        return 0;
    }
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open ENGINE_OTP_FILE: %s\n", path);
        return 0;
    }
    n = fread(otp, 1, sizeof(otp), f);
    extra = fgetc(f);
    fclose(f);
    if (n != sizeof(otp) || extra != EOF) {
        fprintf(stderr, "ENGINE_OTP_FILE must contain exactly 64 bytes\n");
        memset(otp, 0, sizeof(otp));
        return 0;
    }
    return 1;
}

static int copy_raw_frame(void *out, DWORD out_len, const char *path)
{
    enum { RAW = 80 * 64 * 2 };
    FILE *f;
    if (!out || out_len < RAW || !path) return 0;
    f = fopen(path, "rb");
    if (!f || fread(out, 1, RAW, f) != RAW) {
        if (f) fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

static BOOL WINAPI fake_device_io(HANDLE device, DWORD code, LPVOID in,
                                  DWORD in_len, LPVOID out, DWORD out_len,
                                  LPDWORD returned, LPOVERLAPPED ov)
{
    (void)device; (void)in; (void)in_len; (void)ov;
    if (returned) *returned = 0;
    switch (code) {
    case 0x442008: /* GF sensor-info, output is exactly 0x54 bytes */
        if (!out || out_len < 0x54) return FALSE;
        memset(out, 0, out_len);
        memcpy((unsigned char *)out + 1, otp, sizeof(otp));
        *(unsigned *)((unsigned char *)out + 0x44) = 80;       /* col */
        *(unsigned *)((unsigned char *)out + 0x48) = 64;       /* row */
        /* EngineAdapter copies this dword to its LoadAlgorithm selector.
         * 12 is the real chicagoHS sensor type; putting chip 0x2504 here
         * silently selects the unsupported-sensor path. */
        *(unsigned *)((unsigned char *)out + 0x4c) = 12;
        ((unsigned char *)out)[0x50] = 64;                     /* OTP len */
        ((unsigned char *)out)[0x51] = 12;                     /* sensor type */
        if (returned) *returned = 0x54;
        return TRUE;
    case 0x442148: /* PBA info: zero means not supported */
        if (out && out_len) memset(out, 0, out_len);
        if (returned) *returned = 4;
        return TRUE;
    case 0x44214c: /* device info; enough for normal adapter config path */
        if (out && out_len) memset(out, 0, out_len);
        if (returned) *returned = out_len < 0xd4 ? out_len : 0xd4;
        return TRUE;
    case 0x442020: /* driver activate/deactivate acknowledgement */
        if (out && out_len) memset(out, 0, out_len);
        if (returned) *returned = 1;
        return TRUE;
    case 0x442120: { /* sensor-side no-finger preprocessing base */
        enum { RAW = 80 * 64 * 2 };
        if (!out || out_len < RAW || !fake_sensor_base_raw) return FALSE;
        if (!copy_raw_frame(out, out_len, fake_sensor_base_raw)) return FALSE;
        if (returned) *returned = RAW;
        printf("fake DeviceIoControl base frame: %s\n", fake_sensor_base_raw);
        return TRUE;
    }
    case 0x442140: /* RetryCaptureIMG: 6-byte status + vendor capture */
        if (!out || out_len < 6 || !fake_retry_raw) return FALSE;
        memset(out, 0, out_len);
        /* gfspi!OnRetryCaptureIMG writes the status at +4/+5 and the vendor
         * capture payload starts at +6.  Its data source is selected by the
         * three-byte request EngineAdapter sends; this offline substitute
         * supplies the current captured 80x64x16 frame at that exact point. */
        if (!copy_raw_frame((unsigned char *)out + 6, out_len - 6, fake_retry_raw))
            return FALSE;
        ((unsigned char *)out)[4] = 1;
        ((unsigned char *)out)[5] = 1;
        if (returned) *returned = out_len;
        printf("fake DeviceIoControl RetryCaptureIMG: %s\n", fake_retry_raw);
        return TRUE;
    default:
        printf("fake DeviceIoControl: unhandled code=0x%08lx out=%lu\n",
               (unsigned long)code, (unsigned long)out_len);
        SetLastError(ERROR_INVALID_FUNCTION);
        return FALSE;
    }
}

static void print_wide_path(const char *which, LPCWSTR path)
{
    char buf[512];
    int n = path ? WideCharToMultiByte(CP_UTF8, 0, path, -1, buf, sizeof(buf), NULL, NULL) : 0;
    printf("%s(%s)\n", which, n ? buf : "<null>");
}

static void print_hex_prefix(const char *which, const unsigned char *p, size_t n)
{
    size_t i;
    printf("%s", which);
    for (i = 0; i < n; ++i) printf("%02x", p[i]);
    putchar('\n');
}

static HMODULE WINAPI fake_load_library_w(LPCWSTR path)
{
    HMODULE m;
    print_wide_path("EngineAdapter LoadLibraryW", path);
    /* Redirect the vendor load to an explicit oracle DLL when requested.  The
     * basename fallback resolves beside this executable and avoids depending
     * on a mounted Windows DriverStore. */
    if (path && wcsstr(path, L"AlgoChicago.dll")) {
        WCHAR configured[MAX_PATH];
        const char *configured_utf8 = getenv("CHICAGO_DLL");
        const WCHAR *redirect = L"AlgoChicago.dll";

        if (configured_utf8 && *configured_utf8 &&
            MultiByteToWideChar(CP_UTF8, 0, configured_utf8, -1,
                                configured, MAX_PATH) > 0)
            redirect = configured;
        print_wide_path("  redirected AlgoChicago", redirect);
        m = real_load_library_w ? real_load_library_w(redirect) : NULL;
    } else {
        m = real_load_library_w ? real_load_library_w(path) : NULL;
    }
    printf("  -> %p err=%lu\n", m, GetLastError());
    return m;
}

static HMODULE WINAPI fake_load_library_ex_w(LPCWSTR path, HANDLE file, DWORD flags)
{
    HMODULE m;
    print_wide_path("EngineAdapter LoadLibraryExW", path);
    m = real_load_library_ex_w ? real_load_library_ex_w(path, file, flags) : NULL;
    printf("  -> %p err=%lu\n", m, GetLastError());
    return m;
}

static HANDLE WINAPI fake_create_file_w(LPCWSTR path, DWORD access, DWORD share,
                                        LPSECURITY_ATTRIBUTES sa,
                                        DWORD creation, DWORD flags,
                                        HANDLE template_file)
{
    /* Let the real EngineAdapter parse/load the captured calibration.  Only
     * reads are redirected; writes remain inside Wine and never alter /mnt. */
    if (path && (access & GENERIC_READ) && wcsstr(path, L"goodix_calib.dat")) {
        static const WCHAR calibration[] =
            L"Z:\\mnt\\win3\\ProgramData\\Goodix\\goodix_calib.dat";
        WCHAR configured[MAX_PATH];
        const char *configured_utf8 = getenv("CHICAGO_CAL");
        const WCHAR *redirect = calibration;

        if (configured_utf8 && *configured_utf8 &&
            MultiByteToWideChar(CP_UTF8, 0, configured_utf8, -1,
                                configured, MAX_PATH) > 0)
            redirect = configured;
        print_wide_path("EngineAdapter CreateFileW(calibration)", path);
        return real_create_file_w ? real_create_file_w(redirect, access, share,
                                                        sa, creation, flags,
                                                        template_file)
                                  : INVALID_HANDLE_VALUE;
    }
    return real_create_file_w ? real_create_file_w(path, access, share, sa,
                                                    creation, flags, template_file)
                              : INVALID_HANDLE_VALUE;
}

static int hook_device_io(HMODULE module)
{
    unsigned char *base = (unsigned char *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name; ++imp) {
        IMAGE_THUNK_DATA64 *orig, *first;
        const char *dll = (const char *)(base + imp->Name);
        if (_stricmp(dll, "KERNEL32.dll") != 0) continue;
        orig = (IMAGE_THUNK_DATA64 *)(base + imp->OriginalFirstThunk);
        first = (IMAGE_THUNK_DATA64 *)(base + imp->FirstThunk);
        for (; orig->u1.AddressOfData; ++orig, ++first) {
            IMAGE_IMPORT_BY_NAME *name;
            DWORD old;
            if (IMAGE_SNAP_BY_ORDINAL64(orig->u1.Ordinal)) continue;
            name = (IMAGE_IMPORT_BY_NAME *)(base + orig->u1.AddressOfData);
            if (strcmp((const char *)name->Name, "DeviceIoControl") != 0) continue;
            if (!VirtualProtect(&first->u1.Function, sizeof(first->u1.Function), PAGE_READWRITE, &old))
                return 0;
            first->u1.Function = (ULONGLONG)(ULONG_PTR)fake_device_io;
            VirtualProtect(&first->u1.Function, sizeof(first->u1.Function), old, &old);
            return 1;
        }
    }
    return 0;
}

static int hook_loaders(HMODULE module)
{
    unsigned char *base = (unsigned char *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    int hooks = 0;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name; ++imp) {
        IMAGE_THUNK_DATA64 *orig, *first;
        const char *dll = (const char *)(base + imp->Name);
        if (_stricmp(dll, "KERNEL32.dll") != 0) continue;
        orig = (IMAGE_THUNK_DATA64 *)(base + imp->OriginalFirstThunk);
        first = (IMAGE_THUNK_DATA64 *)(base + imp->FirstThunk);
        for (; orig->u1.AddressOfData; ++orig, ++first) {
            IMAGE_IMPORT_BY_NAME *name;
            DWORD old;
            void *replacement = NULL;
            if (IMAGE_SNAP_BY_ORDINAL64(orig->u1.Ordinal)) continue;
            name = (IMAGE_IMPORT_BY_NAME *)(base + orig->u1.AddressOfData);
            if (!strcmp((const char *)name->Name, "LoadLibraryW")) {
                real_load_library_w = (load_library_w_t)(ULONG_PTR)first->u1.Function;
                replacement = fake_load_library_w;
            } else if (!strcmp((const char *)name->Name, "LoadLibraryExW")) {
                real_load_library_ex_w = (load_library_ex_w_t)(ULONG_PTR)first->u1.Function;
                replacement = fake_load_library_ex_w;
            } else if (!strcmp((const char *)name->Name, "CreateFileW")) {
                real_create_file_w = (create_file_w_t)(ULONG_PTR)first->u1.Function;
                replacement = fake_create_file_w;
            }
            if (!replacement) continue;
            if (!VirtualProtect(&first->u1.Function, sizeof(first->u1.Function), PAGE_READWRITE, &old))
                continue;
            first->u1.Function = (ULONGLONG)(ULONG_PTR)replacement;
            VirtualProtect(&first->u1.Function, sizeof(first->u1.Function), old, &old);
            hooks++;
        }
    }
    return hooks;
}

/* One-shot software breakpoints on every AcceptSampleData path that writes
 * WINBIO_FP_POOR_QUALITY (reject detail 10).  This pins a BIR failure to its
 * exact EngineAdapter branch without modifying behaviour after the report. */
static LONG CALLBACK reject_veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *ctx = ep->ContextRecord;
    unsigned char *hit, *previous;
    unsigned i;
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    /* Wine reports RIP at the int3 byte; native Windows commonly reports it
     * after the byte.  Accept either convention. */
    hit = (unsigned char *)(ULONG_PTR)ctx->Rip;
    previous = hit - 1;
    for (i = 0; i < reject_bp_count; i++) {
        if (reject_bps[i].address == hit || reject_bps[i].address == previous) {
            DWORD old;
            if (reject_bps[i].address == previous) hit = previous;
            printf("AcceptSampleData reject=10 at EngineAdapter RVA 0x%tx\n",
                   (ptrdiff_t)(hit - (unsigned char *)GetModuleHandleA("EngineAdapter.dll")));
            if ((ptrdiff_t)(hit - (unsigned char *)GetModuleHandleA("EngineAdapter.dll")) == 0x35bbc) {
                unsigned char *current = *(unsigned char **)(ULONG_PTR)(ctx->Rsp + 0xd0);
                void *engine_ctx = *(void **)(ULONG_PTR)(ctx->Rsp + 0xc0);
                unsigned char *limits = engine_ctx ? *(unsigned char **)engine_ctx : NULL;
                printf("  reject-7 current=%p current[42c,42d]=%u,%u engine limits[30,34]=%u,%u\n",
                       current, current ? current[0x42c] : 0, current ? current[0x42d] : 0,
                       limits ? *(unsigned *)(limits + 0x30) : 0,
                       limits ? *(unsigned *)(limits + 0x34) : 0);
            }
            VirtualProtect(hit, 1, PAGE_EXECUTE_READWRITE, &old);
            *hit = reject_bps[i].original;
            VirtualProtect(hit, 1, old, &old);
            ctx->Rip = (DWORD64)(ULONG_PTR)hit;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void trace_reject_sites(HMODULE module)
{
    static const unsigned rvas[] = {
        0x346d0, 0x34893, 0x3534e, 0x35384, 0x35bbc, 0x35eb7, 0x360d6
    };
    unsigned i;
    AddVectoredExceptionHandler(1, reject_veh);
    for (i = 0; i < sizeof(rvas)/sizeof(rvas[0]); i++) {
        DWORD old;
        unsigned char *p = (unsigned char *)module + rvas[i];
        if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) continue;
        reject_bps[reject_bp_count].address = p;
        reject_bps[reject_bp_count].original = *p;
        reject_bp_count++;
        *p = 0xcc;
        VirtualProtect(p, 1, old, &old);
    }
}

/* Trace the post-call points in _AdapterInitPreprocessor.  These reveal which
 * real Chicago primitive leaves the engine's basevalid flag clear without
 * touching its return value or branch decisions. */
typedef struct { unsigned char *address, original; const char *label; } init_bp_t;
static init_bp_t init_bps[16];
static unsigned init_bp_count;
static void *traced_engine_context;

static LONG CALLBACK init_veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *ctx = ep->ContextRecord;
    unsigned char *hit, *previous;
    unsigned i;
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
        return EXCEPTION_CONTINUE_SEARCH;
    hit = (unsigned char *)(ULONG_PTR)ctx->Rip;
    previous = hit - 1;
    for (i = 0; i < init_bp_count; ++i) {
        if (init_bps[i].address == hit || init_bps[i].address == previous) {
            DWORD old;
            if (init_bps[i].address == previous) hit = previous;
            printf("_AdapterInitPreprocessor %s: rax=0x%llx basevalid=%u\n",
                   init_bps[i].label, (unsigned long long)ctx->Rax,
                   traced_engine_context ?
                       *((unsigned char *)traced_engine_context + 0x2a8) : 0);
            if (!strcmp(init_bps[i].label, "before preprocessor_init")) {
                unsigned char *p = (unsigned char *)(ULONG_PTR)(ctx->Rsp + 0x98);
                const unsigned short *raw = *(const unsigned short **)(p + 0x18);
                printf("  preprocessor_init arg: raw=%p col=%u row=%u fields[0,8,10]=%llx,%llx,%llx\n",
                       *(void **)(p + 0x18), *(unsigned *)(p + 0x24),
                       *(unsigned *)(p + 0x28),
                       (unsigned long long)*(ULONGLONG *)(p + 0x00),
                       (unsigned long long)*(ULONGLONG *)(p + 0x08),
                       (unsigned long long)*(ULONGLONG *)(p + 0x10));
                if (raw)
                    printf("  parsed raw first pixels: %u,%u,%u,%u\n",
                           raw[0], raw[1], raw[2], raw[3]);
            }
            if (!strcmp(init_bps[i].label, "before BIR raw copy")) {
                const unsigned short *raw = (const unsigned short *)(ULONG_PTR)ctx->Rdx;
                printf("  BIR raw copy: source=%p first pixels=%u,%u,%u,%u bytes=%llu\n",
                       raw, raw ? raw[0] : 0, raw ? raw[1] : 0,
                       raw ? raw[2] : 0, raw ? raw[3] : 0,
                       (unsigned long long)ctx->R8);
            }
            if (!strcmp(init_bps[i].label, "direct init path") &&
                getenv("INJECT_BASE") && fake_sensor_base_raw) {
                enum { RAW = 80 * 64 * 2 };
                FILE *f = fopen(fake_sensor_base_raw, "rb");
                if (f && fread((void *)(ULONG_PTR)ctx->Rdx, 1, RAW, f) == RAW)
                    printf("  injected captured base into EngineAdapter raw buffer\n");
                if (f) fclose(f);
            }
            if (!strcmp(init_bps[i].label, "before Chicago input validation") ||
                !strcmp(init_bps[i].label, "after Chicago input validation")) {
                printf("  Chicago regs: rcx=%p rdx=0x%llx r8=0x%llx r9=0x%llx rbp=0x%llx rsi=0x%llx\n",
                       (void *)(ULONG_PTR)ctx->Rcx, (unsigned long long)ctx->Rdx,
                       (unsigned long long)ctx->R8, (unsigned long long)ctx->R9,
                       (unsigned long long)ctx->Rbp, (unsigned long long)ctx->Rsi);
            }
            if (!strcmp(init_bps[i].label, "before Chicago templateUnPack")) {
                const unsigned char *blob = (const unsigned char *)(ULONG_PTR)ctx->Rcx;
                printf("  templateUnPack packed=%p bytes=%llu out-size=%llu out=%p first16=",
                       blob, (unsigned long long)ctx->Rdx,
                       (unsigned long long)ctx->R8, (void *)(ULONG_PTR)ctx->R9);
                for (unsigned j = 0; blob && j < 16 && j < ctx->Rdx; ++j) printf("%02x", blob[j]);
                putchar('\n');
            }
            if (!strcmp(init_bps[i].label, "before Chicago identifyImageWrapper")) {
                const void *const *array = (const void *const *)(ULONG_PTR)ctx->R8;
                printf("  identifyImageWrapper current=%p feature=%p templates=%p first-template=%p "
                       "count=%llu idx@%p score@%p\n",
                       (void *)(ULONG_PTR)ctx->Rcx, (void *)(ULONG_PTR)ctx->Rdx,
                       array, array ? array[0] : NULL, (unsigned long long)ctx->R9,
                       (void *)(ULONG_PTR)*(ULONGLONG *)(ULONG_PTR)(ctx->Rsp + 0x28),
                       (void *)(ULONG_PTR)*(ULONGLONG *)(ULONG_PTR)(ctx->Rsp + 0x30));
            }
            if (!strcmp(init_bps[i].label, "before Chicago identifytemplate")) {
                const unsigned char *gallery = (const unsigned char *)(ULONG_PTR)ctx->Rcx;
                const unsigned char *probe = (const unsigned char *)(ULONG_PTR)ctx->Rdx;
                printf("  identifytemplate gallery=%p probe=%p opaque=%p idx@%p ",
                       gallery, probe, (void *)(ULONG_PTR)ctx->R8,
                       (void *)(ULONG_PTR)ctx->R9);
                printf("gallery16=");
                for (unsigned j = 0; gallery && j < 16; ++j) printf("%02x", gallery[j]);
                printf(" probe16=");
                for (unsigned j = 0; probe && j < 16; ++j) printf("%02x", probe[j]);
                putchar('\n');
            }
            VirtualProtect(hit, 1, PAGE_EXECUTE_READWRITE, &old);
            *hit = init_bps[i].original;
            VirtualProtect(hit, 1, old, &old);
            ctx->Rip = (DWORD64)(ULONG_PTR)hit;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void trace_algo_init(HMODULE algo, void *engine_context)
{
    static const struct { unsigned rva; const char *label; } sites[] = {
        { 0x47d35, "before Chicago input validation" },
        { 0x47d3a, "after Chicago input validation" }
    };
    unsigned i;
    traced_engine_context = engine_context;
    AddVectoredExceptionHandler(1, init_veh);
    for (i = 0; i < sizeof(sites)/sizeof(sites[0]); ++i) {
        DWORD old;
        unsigned char *p = (unsigned char *)algo + sites[i].rva;
        if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) continue;
        init_bps[init_bp_count].address = p;
        init_bps[init_bp_count].original = *p;
        init_bps[init_bp_count].label = sites[i].label;
        init_bp_count++;
        *p = 0xcc;
        VirtualProtect(p, 1, old, &old);
    }
}

static void trace_chicago_unpack(HMODULE algo, void *engine_context)
{
    DWORD old;
    unsigned char *p = (unsigned char *)algo + 0xe2a0; /* templateUnPack */
    traced_engine_context = engine_context;
    AddVectoredExceptionHandler(1, init_veh);
    if (init_bp_count >= sizeof(init_bps)/sizeof(init_bps[0]) ||
        !VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) return;
    init_bps[init_bp_count].address = p;
    init_bps[init_bp_count].original = *p;
    init_bps[init_bp_count].label = "before Chicago templateUnPack";
    init_bp_count++;
    *p = 0xcc;
    VirtualProtect(p, 1, old, &old);
}

static void trace_chicago_identify(HMODULE algo, void *engine_context)
{
    DWORD old;
    unsigned char *p = (unsigned char *)algo + 0xbc40; /* identifyImageWrapper */
    traced_engine_context = engine_context;
    AddVectoredExceptionHandler(1, init_veh);
    if (init_bp_count >= sizeof(init_bps)/sizeof(init_bps[0]) ||
        !VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) return;
    init_bps[init_bp_count].address = p;
    init_bps[init_bp_count].original = *p;
    init_bps[init_bp_count].label = "before Chicago identifyImageWrapper";
    init_bp_count++;
    *p = 0xcc;
    VirtualProtect(p, 1, old, &old);
}

static void trace_chicago_identifytemplate(HMODULE algo, void *engine_context)
{
    DWORD old;
    unsigned char *p = (unsigned char *)algo + 0xdec0; /* identifytemplate */
    traced_engine_context = engine_context;
    AddVectoredExceptionHandler(1, init_veh);
    if (init_bp_count >= sizeof(init_bps)/sizeof(init_bps[0]) ||
        !VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) return;
    init_bps[init_bp_count].address = p;
    init_bps[init_bp_count].original = *p;
    init_bps[init_bp_count].label = "before Chicago identifytemplate";
    init_bp_count++;
    *p = 0xcc;
    VirtualProtect(p, 1, old, &old);
}

static void trace_init_sites(HMODULE module, void *engine_context)
{
    static const struct { unsigned rva; const char *label; } sites[] = {
        { 0x350cc, "before BIR raw copy" },
        { 0x347c5, "base-frame init path" },
        { 0x3528e, "direct init path" },
        { 0x3108b, "after preprocess_get_calidata_len" },
        { 0x311bc, "after sensor-ID compare" },
        { 0x31365, "after preprocess_load_calidata" },
        { 0x31462, "after fallback preprocess_init_calidata" },
        { 0x314c3, "before preprocessor_init" },
        { 0x314d1, "after preprocessor_init" },
        { 0x3152e, "after retry preprocessor_init" },
        { 0x3157d, "before basevalid success" },
        { 0x3158b, "before basevalid success" }
    };
    unsigned i;
    traced_engine_context = engine_context;
    AddVectoredExceptionHandler(1, init_veh);
    for (i = 0; i < sizeof(sites)/sizeof(sites[0]); ++i) {
        DWORD old;
        unsigned char *p = (unsigned char *)module + sites[i].rva;
        if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) continue;
        init_bps[init_bp_count].address = p;
        init_bps[init_bp_count].original = *p;
        init_bps[init_bp_count].label = sites[i].label;
        init_bp_count++;
        *p = 0xcc;
        VirtualProtect(p, 1, old, &old);
    }
}

typedef struct {
    HANDLE SensorHandle, EngineHandle, StorageHandle;
    void *SensorInterface, *EngineInterface, *StorageInterface;
    void *SensorContext, *EngineContext, *StorageContext, *FrameworkInterface;
} pipeline_t;

/* The real EngineAdapter calls the WBF storage-interface table directly when
 * it commits an enrollment and when it identifies.  This tiny in-memory table
 * is therefore an exact boundary substitute: it never invents matcher
 * results, it just holds the template blob that the untouched engine emits. */
typedef struct {
    adapter_version_t Version;
    unsigned Type;
    size_t Size;
    GUID AdapterId;
    void *Attach, *Detach, *ClearContext;
    void *CreateDatabase, *EraseDatabase, *OpenDatabase, *CloseDatabase;
    void *GetDataFormat, *GetDatabaseSize;
    void *AddRecord, *DeleteRecord, *QueryBySubject, *QueryByContent;
    void *GetRecordCount, *FirstRecord, *NextRecord, *GetCurrentRecord;
    void *ControlUnit, *ControlUnitPrivileged;
} storage_interface_t;

typedef struct {
    unsigned Type;
    union {
        unsigned Null;
        unsigned Wildcard;
        GUID TemplateGuid;
        struct { unsigned Size; unsigned char Data[68]; } AccountSid;
    } Value;
} identity_t;

typedef struct {
    identity_t *Identity;
    unsigned char SubFactor;
    unsigned char _pad0[7];
    unsigned *IndexVector;
    size_t IndexElementCount;
    unsigned char *TemplateBlob;
    size_t TemplateBlobSize;
    unsigned char *PayloadBlob;
    size_t PayloadBlobSize;
} storage_record_t;

static unsigned char *stored_template;
static size_t stored_template_size;
static identity_t stored_identity;
static unsigned char stored_subfactor;

typedef HRESULT (WINAPI *storage_query_content_t)(pipeline_t *, unsigned char,
                                                  unsigned *, size_t);
typedef HRESULT (WINAPI *storage_count_t)(pipeline_t *, size_t *);
typedef HRESULT (WINAPI *storage_cursor_t)(pipeline_t *);
typedef HRESULT (WINAPI *storage_current_t)(pipeline_t *, storage_record_t *);
typedef HRESULT (WINAPI *storage_add_t)(pipeline_t *, const storage_record_t *);

static HRESULT WINAPI fake_storage_query_content(pipeline_t *p, unsigned char subtype,
                                                 unsigned *vector, size_t count)
{
    (void)p; (void)subtype; (void)vector; (void)count;
    printf("fake storage QueryByContent: %zu-byte template available\n", stored_template_size);
    return stored_template ? S_OK : (HRESULT)0x8009801fL; /* DATABASE_NO_RESULTS */
}

static HRESULT WINAPI fake_storage_get_record_count(pipeline_t *p, size_t *count)
{
    (void)p;
    if (!count) return E_POINTER;
    *count = stored_template ? 1 : 0;
    printf("fake storage GetRecordCount -> %zu\n", *count);
    return S_OK;
}

static HRESULT WINAPI fake_storage_first_record(pipeline_t *p)
{
    (void)p;
    printf("fake storage FirstRecord\n");
    return stored_template ? S_OK : (HRESULT)0x8009801fL;
}

static HRESULT WINAPI fake_storage_next_record(pipeline_t *p)
{
    (void)p;
    printf("fake storage NextRecord\n");
    return (HRESULT)0x80098020L; /* DATABASE_NO_MORE_RECORDS */
}

static HRESULT WINAPI fake_storage_get_current_record(pipeline_t *p, storage_record_t *out)
{
    (void)p;
    if (!out) return E_POINTER;
    if (!stored_template) return (HRESULT)0x8009801fL;
    memset(out, 0, sizeof(*out));
    out->Identity = &stored_identity;
    out->SubFactor = stored_subfactor;
    out->TemplateBlob = stored_template;
    out->TemplateBlobSize = stored_template_size;
    printf("fake storage GetCurrentRecord: blob=%p size=%zu\n",
           stored_template, stored_template_size);
    return S_OK;
}

static HRESULT WINAPI fake_storage_add_record(pipeline_t *p, const storage_record_t *record)
{
    (void)p;
    if (!record || !record->TemplateBlob || !record->TemplateBlobSize) return E_POINTER;
    free(stored_template);
    stored_template = (unsigned char *)malloc(record->TemplateBlobSize);
    if (!stored_template) return E_OUTOFMEMORY;
    memcpy(stored_template, record->TemplateBlob, record->TemplateBlobSize);
    stored_template_size = record->TemplateBlobSize;
    stored_identity = record->Identity ? *record->Identity : (identity_t){0};
    stored_subfactor = record->SubFactor;
    printf("fake storage AddRecord: captured %zu-byte EngineAdapter template\n",
           stored_template_size);
    { const char *path = getenv("DUMP_STORED");
      if (path) {
          FILE *f = fopen(path, "wb");
          if (f) { fwrite(stored_template, 1, stored_template_size, f); fclose(f); }
          printf("fake storage template written to %s\n", path);
      } }
    return S_OK;
}

static storage_interface_t fake_storage = {
    { 3, 0 }, 3, sizeof(fake_storage), { 0 },
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    (void *)fake_storage_add_record, NULL, NULL, (void *)fake_storage_query_content,
    (void *)fake_storage_get_record_count, (void *)fake_storage_first_record,
    (void *)fake_storage_next_record, (void *)fake_storage_get_current_record,
    NULL, NULL
};

typedef HRESULT (WINAPI *attach_t)(pipeline_t *);
typedef HRESULT (WINAPI *detach_t)(pipeline_t *);
typedef HRESULT (WINAPI *accept_t)(pipeline_t *, const void *, size_t,
                                   unsigned char, unsigned *);
typedef HRESULT (WINAPI *create_enrollment_t)(pipeline_t *);
typedef HRESULT (WINAPI *update_enrollment_t)(pipeline_t *, unsigned *);
typedef HRESULT (WINAPI *get_enrollment_status_t)(pipeline_t *, unsigned *);
typedef HRESULT (WINAPI *commit_enrollment_t)(pipeline_t *, const identity_t *,
                                               unsigned char, unsigned char *, size_t);
typedef HRESULT (WINAPI *identify_feature_t)(pipeline_t *, identity_t *, unsigned char *,
                                             unsigned char **, size_t *, unsigned char **,
                                             size_t *, unsigned *);
typedef HRESULT (WINAPI *sample_hint_t)(pipeline_t *, size_t *);
typedef int (*chicago_preprocess_t)(void *, void *, void *, void *, void *, char, char);
typedef void *(*chicago_enrol_start_t)(void);
typedef int (*chicago_enrol_add_t)(void *, void *, void *, void *, char, void *);
typedef int (*chicago_enrol_get_t)(void *, void **);
typedef int (*chicago_template_size_t)(void *);
typedef int (*chicago_template_pack_t)(void *, void *);
typedef int (*algo_int0_t)(void);
typedef int (*algo_ppp_t)(int);
typedef int (*algo_load_cal_t)(void *, unsigned);
typedef unsigned (*algo_cal_len_t)(void);

/* Chicago stores [16-byte sensor ID][one 0x224b0-byte calibration blob].
 * This direct loader is an ABI control.  ENGINE_OWN_CALIB skips it so the
 * real EngineAdapter file-init path is the thing being tested. */
static int preload_real_calibration(void)
{
    static const char default_path[] =
        "Z:\\mnt\\win3\\ProgramData\\Goodix\\goodix_calib.dat";
    const char *path = getenv("CHICAGO_CAL");
    HMODULE algo = GetModuleHandleA("AlgoChicago.dll");
    FILE *f;
    unsigned char *blob;
    algo_ppp_t ppp;
    algo_int0_t init;
    algo_load_cal_t load;
    algo_cal_len_t cal_len;
    unsigned len = 0;
    int ret;
    if (!algo) return -1;
    ppp = (algo_ppp_t)GetProcAddress(algo, "ppp_param_init");
    init = (algo_int0_t)GetProcAddress(algo, "preprocess_init_calidata");
    load = (algo_load_cal_t)GetProcAddress(algo, "preprocess_load_calidata");
    cal_len = (algo_cal_len_t)GetProcAddress(algo, "preprocess_get_calidata_len");
    if (!path || !*path) path = default_path;
    f = fopen(path, "rb");
    if (!ppp || !init || !load || !cal_len || !f) return -2;
    ppp(12);
    init();
    len = cal_len();
    printf("AlgoChicago calidata length: 0x%x (file payload expected after 16-byte ID)\n", len);
    blob = (unsigned char *)calloc(1, len);
    if (!blob) return -3;
    fseek(f, 16, SEEK_SET);
    if (fread(blob, 1, len, f) != len) {
        fclose(f); free(blob); return -4;
    }
    fclose(f);
    ret = load(blob, len);
    free(blob);
    return ret;
}

/* Build one complete, uncompressed ANSI-381 BDB record: BIR at +0, BIR header
 * at +0x20, standard BDB at +0x50, 16-bit pixels at BDB+0x38.  In particular
 * the header and record lengths below are real ANSI fields, not only enough
 * bytes to get past EngineAdapter's initial bounds checks. */
static unsigned char *make_sample(const char *path, size_t *sample_size)
{
    enum { RAW = 80 * 64 * 2, BIR = 0x20, HDR = 0x30, BDB = 0x38 };
    const unsigned std_off = BIR + HDR;
    const size_t total = std_off + BDB + RAW;
    unsigned char *p = (unsigned char *)calloc(1, total);
    FILE *f;
    if (!p) return NULL;
    *(unsigned *)(p + 0x00) = HDR;       /* HeaderBlock.Size */
    *(unsigned *)(p + 0x04) = BIR;       /* HeaderBlock.Offset */
    *(unsigned *)(p + 0x08) = BDB + RAW; /* StandardDataBlock.Size */
    *(unsigned *)(p + 0x0c) = std_off;   /* StandardDataBlock.Offset */
    *(ULONGLONG *)(p + std_off + 0x00) = BDB + RAW; /* ANSI RecordLength */
    *(unsigned *)(p + std_off + 0x08) = 0x46495200; /* "FIR\0" */
    *(unsigned *)(p + std_off + 0x0c) = 0x30313000; /* ANSI 381 v0.1 */
    *(unsigned short *)(p + std_off + 0x10) = 0x1b;  /* ANSI 381 owner */
    *(unsigned short *)(p + std_off + 0x12) = 0x401; /* ANSI 381 type */
    *(unsigned short *)(p + BIR + 0x28) = 0x1b;  /* ANSI 381 owner */
    *(unsigned short *)(p + BIR + 0x2a) = 0x401; /* ANSI 381 type */
    *(unsigned short *)(p + std_off + 0x14) = 1;   /* capture device */
    *(unsigned short *)(p + std_off + 0x16) = 1;   /* acquisition level */
    *(unsigned short *)(p + std_off + 0x18) = 500; /* horizontal scan dpi */
    *(unsigned short *)(p + std_off + 0x1a) = 500; /* vertical scan dpi */
    *(unsigned short *)(p + std_off + 0x1c) = 500; /* horizontal image dpi */
    *(unsigned short *)(p + std_off + 0x1e) = 500; /* vertical image dpi */
    p[std_off + 0x20] = 1;               /* one ANSI record */
    p[std_off + 0x21] = 1;               /* scale units: inch */
    p[std_off + 0x22] = 16;              /* bits per pixel */
    p[std_off + 0x23] = 0;               /* uncompressed */
    *(unsigned *)(p + std_off + 0x28) = 0x10 + RAW; /* record BlockLength */
    *(unsigned short *)(p + std_off + 0x2c) = 80;
    *(unsigned short *)(p + std_off + 0x2e) = 64;
    p[std_off + 0x30] = 2;               /* right index finger */
    p[std_off + 0x31] = 1;               /* count of views */
    p[std_off + 0x32] = 1;               /* view number */
    p[std_off + 0x33] = 0xfe;            /* ANSI required quality placeholder */
    f = fopen(path, "rb");
    if (!f || fread(p + std_off + BDB, 1, RAW, f) != RAW) {
        if (f) fclose(f);
        free(p);
        return NULL;
    }
    fclose(f);
    *sample_size = total;
    return p;
}

static HRESULT engine_accept_one(engine_interface_t *iface, pipeline_t *pipeline,
                                 const char *path, unsigned *reject)
{
    size_t sample_size = 0;
    unsigned char *sample = make_sample(path, &sample_size);
    HRESULT hr;
    if (!sample) {
        printf("could not make sample from %s\n", path);
        return E_FAIL;
    }
    fake_retry_raw = path;
    *reject = 0;
    hr = ((accept_t)iface->AcceptSampleData)(pipeline, sample, sample_size, 2, reject);
    printf("AcceptSampleData %s hr=0x%08lx reject=%u sample=%zu\n",
           path, (unsigned long)hr, *reject, sample_size);
    free(sample);
    return hr;
}

static int load_stored_template(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return 0;
    }
    free(stored_template);
    stored_template = (unsigned char *)malloc((size_t)size);
    if (!stored_template || fread(stored_template, 1, (size_t)size, f) != (size_t)size) {
        free(stored_template); stored_template = NULL; stored_template_size = 0;
        fclose(f);
        return 0;
    }
    fclose(f);
    stored_template_size = (size_t)size;
    memset(&stored_identity, 0, sizeof(stored_identity));
    stored_identity.Type = 2; /* WINBIO_ID_TYPE_GUID */
    stored_identity.Value.TemplateGuid.Data1 = 0x51c0f00d;
    stored_subfactor = 2;
    printf("loaded %zu-byte packed AlgoChicago template from %s\n",
           stored_template_size, path);
    return 1;
}

/* Build a Chicago packed template after EngineAdapter has already performed
 * the real calibration/base initialization.  Unlike match.exe's standalone
 * control, this deliberately reuses the loaded algorithm's live state. */
static int build_live_chicago_template(const char *path)
{
    enum { RAW = 80 * 64 * 2, IMG = 80 * 64, HDR = 0x40 };
    HMODULE algo = GetModuleHandleA("AlgoChicago.dll");
    chicago_preprocess_t preprocess;
    chicago_enrol_start_t enrol_start;
    chicago_enrol_add_t enrol_add;
    chicago_enrol_get_t enrol_get;
    chicago_template_size_t packed_size;
    chicago_template_pack_t pack;
    unsigned short *raw;
    unsigned char *enh;
    unsigned char *in, *out, *qc;
    void *a1, *a2, *ea2, *ea3, *meta, *enroll_ctx, *template_handle = NULL;
    int quality, coverage, size, ret;

    if (!algo) return 0;
    preprocess = (chicago_preprocess_t)GetProcAddress(algo, "preprocessor");
    enrol_start = (chicago_enrol_start_t)GetProcAddress(algo, "enrolStart");
    enrol_add = (chicago_enrol_add_t)GetProcAddress(algo, "enrolAddImage");
    enrol_get = (chicago_enrol_get_t)GetProcAddress(algo, "enrolGetTemplate");
    packed_size = (chicago_template_size_t)GetProcAddress(algo, "templateGetPackedSize");
    pack = (chicago_template_pack_t)GetProcAddress(algo, "templatePack");
    if (!preprocess || !enrol_start || !enrol_add || !enrol_get || !packed_size || !pack)
        return 0;
    raw = (unsigned short *)calloc(1, RAW);
    enh = (unsigned char *)calloc(1, IMG);
    in = (unsigned char *)calloc(1, HDR);
    out = (unsigned char *)calloc(1, HDR);
    qc = (unsigned char *)calloc(1, 0x20);
    a1 = calloc(1, 0x8000); a2 = calloc(1, 0x8000);
    ea2 = calloc(1, 0x8000); ea3 = calloc(1, 0x8000); meta = calloc(1, 0x10000);
    if (!raw || !enh || !in || !out || !qc || !a1 || !a2 || !ea2 || !ea3 || !meta)
        return 0;
    if (!copy_raw_frame(raw, RAW, path)) return 0;
    *(void **)(in + 0x00) = raw;
    *(unsigned short *)(in + 0x08) = 64;
    *(unsigned short *)(in + 0x0a) = 80;
    *(unsigned *)(in + 0x14) = RAW;
    *(unsigned short *)(in + 0x18) = 1;
    *(void **)(out + 0x00) = enh;
    *(unsigned short *)(out + 0x08) = 64;
    *(unsigned short *)(out + 0x0a) = 80;
    *(unsigned *)(out + 0x14) = IMG;
    ret = preprocess(in, a1, a2, out, qc, 0, 0);
    quality = out[0x28]; coverage = out[0x29];
    out[0x0e] = 8; out[0x0f] = 1; *(unsigned short *)(out + 0x18) = 1;
    enroll_ctx = enrol_start();
    if (!enroll_ctx || ret || enrol_add(enroll_ctx, out, ea2, ea3, 0, meta) ||
        enrol_get(enroll_ctx, &template_handle) || !template_handle)
        return 0;
    size = packed_size(template_handle);
    if (size <= 0) return 0;
    free(stored_template);
    stored_template = (unsigned char *)calloc(1, (size_t)size);
    if (!stored_template || pack(template_handle, stored_template)) {
        free(stored_template); stored_template = NULL; stored_template_size = 0;
        return 0;
    }
    stored_template_size = (size_t)size;
    memset(&stored_identity, 0, sizeof(stored_identity));
    stored_identity.Type = 2;
    stored_identity.Value.TemplateGuid.Data1 = 0x51c0f00d;
    stored_subfactor = 2;
    printf("live Chicago template: q=%d cov=%d packed=%zu from %s\n",
           quality, coverage, stored_template_size, path);
    return 1;
}

static HRESULT engine_identify_stored(engine_interface_t *iface, pipeline_t *pipeline,
                                      const char *probe)
{
    HRESULT hr;
    unsigned reject = 0;
    identity_t matched_id;
    unsigned char matched_subfactor = 0, *payload = NULL, *hash = NULL;
    size_t payload_size = 0, hash_size = 0;
    if (!stored_template) return E_FAIL;
    hr = engine_accept_one(iface, pipeline, probe, &reject);
    if (FAILED(hr)) return hr;
    memset(&matched_id, 0, sizeof(matched_id));
    reject = 0;
    hr = ((identify_feature_t)iface->IdentifyFeatureSet)(pipeline, &matched_id,
                                                           &matched_subfactor, &payload,
                                                           &payload_size, &hash, &hash_size,
                                                           &reject);
    printf("IdentifyFeatureSet(stored) hr=0x%08lx reject=%u id.type=%u "
           "subfactor=%u payload=%zu hash=%zu %s\n",
           (unsigned long)hr, reject, matched_id.Type, matched_subfactor,
           payload_size, hash_size,
           hr == S_OK ? "*** ENGINE MATCH ***" : "no engine match");
    return hr;
}

/* Full Windows-engine round-trip, with only storage persistence replaced by
 * fake_storage.  Use two captured paths: gallery then probe.  The matching,
 * enrollment, preprocessing and template serialization all execute in the
 * unmodified EngineAdapter and AlgoChicago binaries. */
static HRESULT engine_match_roundtrip(engine_interface_t *iface, pipeline_t *pipeline,
                                      const char *const *gallery, size_t gallery_count,
                                      const char *probe)
{
    HRESULT hr;
    unsigned reject = 0;
    size_t hint = 0, i, attempts;
    identity_t enroll_id, matched_id;
    unsigned char subfactor = 0, matched_subfactor = 0;
    unsigned char *payload = NULL, *hash = NULL;
    size_t payload_size = 0, hash_size = 0;

    memset(&enroll_id, 0, sizeof(enroll_id));
    enroll_id.Type = 2; /* WINBIO_ID_TYPE_GUID */
    enroll_id.Value.TemplateGuid.Data1 = 0x51c0f00d;
    enroll_id.Value.TemplateGuid.Data2 = 0x0907;
    enroll_id.Value.TemplateGuid.Data3 = 0x2026;
    enroll_id.Value.TemplateGuid.Data4[0] = 0x47;
    enroll_id.Value.TemplateGuid.Data4[1] = 0x46;
    subfactor = 2; /* ANSI-381 right index finger */

    hr = ((sample_hint_t)iface->QuerySampleHint)(pipeline, &hint);
    printf("QuerySampleHint hr=0x%08lx hint=%zu\n", (unsigned long)hr, hint);
    if (FAILED(hr)) hint = 1;
    attempts = hint ? hint : 1;
    /* Use every supplied real capture up to the Engine's requested sample
     * count.  The earlier eight-sample bound could never complete Chicago's
     * advertised 12-sample enrollment, so it prevented an end-to-end Engine
     * matching experiment by construction. */
    if (attempts > gallery_count) attempts = gallery_count;
    if (getenv("ENGINE_USE_ALL")) attempts = gallery_count;

    hr = ((create_enrollment_t)iface->CreateEnrollment)(pipeline);
    printf("CreateEnrollment hr=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) return hr;
    for (i = 0; i < attempts; ++i) {
        hr = engine_accept_one(iface, pipeline, gallery[i % gallery_count], &reject);
        if (FAILED(hr)) return hr;
        reject = 0;
        if (getenv("TRACE_ENROLL_POLICY"))
            arm_enrollment_policy_trace(GetModuleHandleA("EngineAdapter.dll"),
                                        (unsigned)i + 1);
        hr = ((update_enrollment_t)iface->UpdateEnrollment)(pipeline, &reject);
        printf("UpdateEnrollment[%zu] hr=0x%08lx reject=%u\n",
               i + 1, (unsigned long)hr, reject);
        if (hr == S_OK) break;
        if (FAILED(hr)) {
            /* Diversity failures are normal enrollment feedback.  Let the
             * oracle try later supplied real captures when explicitly asked,
             * so we can recover a valid 12-position enrollment set without
             * pretending that a rejected capture was accepted. */
            if (getenv("ENGINE_CONTINUE_BAD")) continue;
            return hr;
        }
    }
    if (hr != S_OK) {
        printf("enrollment remained incomplete after %zu real samples\n", attempts);
        return hr;
    }
    reject = 0;
    hr = ((get_enrollment_status_t)iface->GetEnrollmentStatus)(pipeline, &reject);
    printf("GetEnrollmentStatus hr=0x%08lx reject=%u\n", (unsigned long)hr, reject);
    if (FAILED(hr)) return hr;
    hr = ((commit_enrollment_t)iface->CommitEnrollment)(pipeline, &enroll_id,
                                                          subfactor, NULL, 0);
    printf("CommitEnrollment hr=0x%08lx stored=%zu\n",
           (unsigned long)hr, stored_template_size);
    if (FAILED(hr)) return hr;

    hr = engine_accept_one(iface, pipeline, probe, &reject);
    if (FAILED(hr)) return hr;
    memset(&matched_id, 0, sizeof(matched_id));
    reject = 0;
    hr = ((identify_feature_t)iface->IdentifyFeatureSet)(pipeline, &matched_id,
                                                           &matched_subfactor, &payload,
                                                           &payload_size, &hash, &hash_size,
                                                           &reject);
    printf("IdentifyFeatureSet hr=0x%08lx reject=%u id.type=%u subfactor=%u "
           "payload=%zu hash=%zu %s\n",
           (unsigned long)hr, reject, matched_id.Type, matched_subfactor,
           payload_size, hash_size,
           hr == S_OK ? "*** ENGINE MATCH ***" : "no engine match");
    return hr;
}

#define SHOW(x) printf("%-34s +0x%03zx = %p\n", #x, offsetof(engine_interface_t, x), iface->x)

int main(int argc, char **argv)
{
    HMODULE m = LoadLibraryA("EngineAdapter.dll");
    if (!m) {
        printf("LoadLibrary(EngineAdapter.dll) failed: %lu\n", GetLastError());
        return 1;
    }
    query_t q = (query_t)GetProcAddress(m, "WbioQueryEngineInterface");
    if (!q) {
        printf("WbioQueryEngineInterface missing: %lu\n", GetLastError());
        return 1;
    }
    engine_interface_t *iface = NULL;
    HRESULT hr = q(&iface);
    printf("query hr=0x%08lx iface=%p sizeof(local)=0x%zx\n",
           (unsigned long)hr, iface, sizeof(*iface));
    if (FAILED(hr) || !iface)
        return 1;
    printf("version=%u.%u type=%u size=0x%zx\n",
           iface->Version.major, iface->Version.minor, iface->Type, iface->Size);
    SHOW(Attach); SHOW(Detach); SHOW(ClearContext); SHOW(QueryPreferredFormat);
    SHOW(QueryIndexVectorSize); SHOW(QueryHashAlgorithms); SHOW(SetHashAlgorithm);
    SHOW(QuerySampleHint); SHOW(AcceptSampleData); SHOW(ExportEngineData);
    SHOW(VerifyFeatureSet); SHOW(IdentifyFeatureSet); SHOW(CreateEnrollment);
    SHOW(UpdateEnrollment); SHOW(GetEnrollmentStatus); SHOW(GetEnrollmentHash);
    SHOW(CheckForDuplicate); SHOW(CommitEnrollment); SHOW(DiscardEnrollment);
    SHOW(ControlUnit); SHOW(ControlUnitPrivileged); SHOW(NotifyPowerChange);
    SHOW(Reserved1); SHOW(PipelineInit); SHOW(PipelineCleanup); SHOW(Activate);
    SHOW(Deactivate); SHOW(QueryExtendedInfo); SHOW(IdentifyAll);
    SHOW(SetEnrollmentSelector); SHOW(SetEnrollmentParameters);
    SHOW(QueryExtendedEnrollmentStatus); SHOW(RefreshCache);
    SHOW(SelectCalibrationFormat); SHOW(QueryCalibrationData); SHOW(SetAccountPolicy);
    SHOW(CreateKey); SHOW(IdentifyFeatureSetSecure); SHOW(AcceptPrivateSensorTypeInfo);
    SHOW(CreateEnrollmentAuthenticated); SHOW(IdentifyFeatureSetAuthenticated);
    if (getenv("ATTACH")) {
        pipeline_t pipeline;
        if (!load_otp_fixture())
            return 2;
        memset(&pipeline, 0, sizeof(pipeline));
        pipeline.SensorHandle = (HANDLE)(ULONG_PTR)1;
        pipeline.EngineHandle = INVALID_HANDLE_VALUE;
        pipeline.EngineInterface = iface;
        printf("hook DeviceIoControl: %s; loader hooks=%d\n",
               hook_device_io(m) ? "ok" : "FAILED", hook_loaders(m));
        hr = ((attach_t)iface->Attach)(&pipeline);
        printf("Attach hr=0x%08lx EngineContext=%p\n", (unsigned long)hr, pipeline.EngineContext);
        if (SUCCEEDED(hr) && pipeline.EngineContext && argc >= 2) {
            size_t sample_size = 0;
            unsigned reject = 0;
            fake_sensor_base_raw = getenv("SENSOR_BASE_RAW");
            if (!fake_sensor_base_raw) fake_sensor_base_raw = argv[1];
            print_hex_prefix("EngineContext+0x51 sensor ID: ",
                             (const unsigned char *)pipeline.EngineContext + 0x51, 16);
            print_hex_prefix("fake sensor ID:               ", otp, 16);
            if (getenv("ENGINE_OWN_CALIB"))
                printf("using EngineAdapter-owned calibration initialization\n");
            else {
                int calibration_ret = preload_real_calibration();
                printf("preload real calibration ret=0x%x\n", calibration_ret);
                if (calibration_ret != 0) {
                    hr = E_FAIL;
                    goto detach_pipeline;
                }
            }
            if (getenv("CALIB_ONLY")) {
                printf("calibration-only smoke test complete\n");
                goto detach_pipeline;
            }
            if (getenv("TRACE_REJECT")) trace_reject_sites(m);
            if (getenv("TRACE_INIT")) trace_init_sites(m, pipeline.EngineContext);
            if (getenv("TRACE_ALGO_INIT"))
                trace_algo_init(GetModuleHandleA("AlgoChicago.dll"), pipeline.EngineContext);
            if (getenv("TRACE_TEMPLATE"))
                trace_chicago_unpack(GetModuleHandleA("AlgoChicago.dll"), pipeline.EngineContext);
            if (getenv("TRACE_IDENTIFY"))
                trace_chicago_identify(GetModuleHandleA("AlgoChicago.dll"), pipeline.EngineContext);
            if (getenv("TRACE_IDENTIFY_TEMPLATE"))
                trace_chicago_identifytemplate(GetModuleHandleA("AlgoChicago.dll"), pipeline.EngineContext);
            pipeline.StorageInterface = &fake_storage;
            if (getenv("ENGINE_LIVE_IDENTIFY")) {
                unsigned warmup_reject = 0;
                if (argc < 3) {
                    printf("ENGINE_LIVE_IDENTIFY requires <gallery.raw> <probe.raw>\n");
                    hr = E_INVALIDARG;
                } else {
                    hr = engine_accept_one(iface, &pipeline, argv[1], &warmup_reject);
                    if (SUCCEEDED(hr) && !build_live_chicago_template(argv[1])) {
                        printf("could not build a live Chicago packed template\n");
                        hr = E_FAIL;
                    }
                    if (SUCCEEDED(hr)) hr = engine_identify_stored(iface, &pipeline, argv[2]);
                }
            } else if (getenv("ENGINE_IDENTIFY")) {
                const char *template_path = getenv("STORED_TEMPLATE");
                if (!template_path || !load_stored_template(template_path)) {
                    printf("ENGINE_IDENTIFY requires a readable STORED_TEMPLATE\n");
                    hr = E_INVALIDARG;
                } else {
                    hr = engine_identify_stored(iface, &pipeline, argv[1]);
                }
            } else if (getenv("ENGINE_MATCH")) {
                unsigned gallery_count = 1;
                const char *gallery_count_text = getenv("ENGINE_GALLERY_COUNT");
                if (gallery_count_text) gallery_count = (unsigned)strtoul(gallery_count_text, NULL, 0);
                if (!gallery_count || argc < (int)gallery_count + 2) {
                    printf("ENGINE_MATCH requires N gallery raws then one probe "
                           "(set ENGINE_GALLERY_COUNT=N)\n");
                    hr = E_INVALIDARG;
                } else {
                    hr = engine_match_roundtrip(iface, &pipeline, (const char *const *)&argv[1],
                                                gallery_count, argv[gallery_count + 1]);
                }
            } else {
                unsigned round, rounds = argc >= 3 ? 2 : (getenv("ACCEPT_TWICE") ? 2 : 1);
                for (round = 0; round < rounds; ++round) {
                    const char *sample_path = argc >= 3 ? argv[round + 1] : argv[1];
                    unsigned char *sample = make_sample(sample_path, &sample_size);
                    if (!sample) {
                        printf("could not make sample from %s\n", sample_path);
                        continue;
                    }
                    fake_retry_raw = sample_path;
                    reject = 0;
                    hr = ((accept_t)iface->AcceptSampleData)(&pipeline, sample,
                                                              sample_size, 2, &reject);
                    printf("AcceptSampleData[%u] %s hr=0x%08lx reject=%u sample=%zu\n",
                           round + 1, sample_path, (unsigned long)hr, reject, sample_size);
                    free(sample);
                }
            }
        }
detach_pipeline:
        if (pipeline.EngineContext)
            printf("Detach hr=0x%08lx\n", (unsigned long)((detach_t)iface->Detach)(&pipeline));
    }
    return 0;
}
