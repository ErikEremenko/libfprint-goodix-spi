// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * Offline production Chicago preprocessor oracle.
 *
 * Uses only AlgoChicago.dll under Wine, never the Linux production driver.
 * The call shape is taken from the successful EngineAdapter path:
 *   ppp_param_init(12) -> init cal -> load [16-byte id][0x224b0 payload]
 *   -> preprocessor_init(raw ImageBase) -> preprocessor(raw frame).
 *
 * Build:
 *   x86_64-w64-mingw32-gcc -O2 chicago_preprocess_dump.c -o chicago_preprocess_dump.exe
 * Run from re/algo-oracle:
 *   WINEDEBUG=-all wine chicago_preprocess_dump.exe <base.raw> <frame.raw> <enh.bin> [meta.bin]
 *
 * CHICAGO_CAL may override the Wine/DOS path to goodix_calib.dat.
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64
#define HEIGHT 80
#define RAW_BYTES (WIDTH * HEIGHT * 2)
#define IMAGE_BYTES (WIDTH * HEIGHT)
#define HEADER_BYTES 0x40

#define PTR(p, off, value) (*(void **) ((unsigned char *) (p) + (off)) = (void *) (value))
#define U16(p, off, value) (*(unsigned short *) ((unsigned char *) (p) + (off)) = (unsigned short) (value))
#define U32(p, off, value) (*(unsigned *) ((unsigned char *) (p) + (off)) = (unsigned) (value))

typedef int (*ppp_param_init_t) (int);
typedef int (*preprocess_init_calidata_t) (void);
typedef unsigned (*preprocess_get_calidata_len_t) (void);
typedef int (*preprocess_load_calidata_t) (void *, unsigned);
typedef int (*preprocessor_init_t) (void *);
typedef int (*preprocess_set_mode_t) (int);
typedef int (*preprocessor_t) (void *, void *, void *, void *, void *, char, char);
typedef void *(*enrol_start_t) (void);
typedef int (*enrol_add_image_t) (void *, void *, void *, void *, char, void *);

#define EXPORT(module, type, name) ((type) (void *) GetProcAddress ((module), (name)))

/*
 * Optional single-shot instruction tap for offline reverse engineering.
 *
 * With CHICAGO_TAP_RVA=48b6b, for example, this stops immediately after the
 * internal 0x47c30 routine returns.  CHICAGO_TAP_REG selects the x64 register
 * to copy (rax, rcx, rdx, r8, r9, r10, r11, r12, r13, r14, r15, rbx, rdi,
 * rsi, rsp, or rbp);
 * CHICAGO_TAP_OUT and CHICAGO_TAP_BYTES
 * select the output file and length. CHICAGO_TAP_RSP_OFFSET plus
 * CHICAGO_TAP_DEREFS can follow stack-held output pointers.
 * CHICAGO_TAP_PRE_ADD_OFFSET indexes before the first dereference, while
 * CHICAGO_TAP_ADD_OFFSET selects a member of the final object. It patches only
 * the mapped Wine process. CHICAGO_TAP_REGISTER_VALUE copies the register's
 * scalar value itself (up to eight bytes) instead of dereferencing it. The tap
 * source can instead be fixed at AlgoChicago+CHICAGO_TAP_MODULE_RVA, which is
 * useful for dumping writable global candidate arrays after a call returns.
 * is deliberately not part of the Linux driver or the normal oracle path.
 * CHICAGO_TAP_OCCURRENCE selects a later invocation while safely single-
 * stepping and rearming the breakpoint for each preceding invocation.
 */
static volatile unsigned char *tap_address;
static unsigned char tap_original_byte;
static volatile LONG tap_armed;
static volatile LONG tap_hits;
static volatile LONG tap_rearm;
static unsigned char *tap_data;
static size_t tap_bytes;
static const char *tap_register;
static unsigned long long tap_pointer;
static long tap_pre_add_offset;
static long tap_rsp_offset;
static long tap_add_offset;
static unsigned tap_derefs;
static unsigned tap_post_derefs;
static unsigned tap_target_occurrence;
static int tap_register_value;
static volatile LONG tap_copy_failed;
static unsigned char *tap_module_base;
static unsigned long tap_module_rva;
static int tap_module_source;

static int
tap_is_readable (const void *address,
                 size_t      bytes)
{
  const unsigned char *cursor = address;

  while (bytes)
    {
      MEMORY_BASIC_INFORMATION memory;
      SIZE_T available;

      if (!VirtualQuery (cursor, &memory, sizeof (memory)) ||
          memory.State != MEM_COMMIT ||
          memory.Protect == PAGE_NOACCESS ||
          memory.Protect & PAGE_GUARD)
        return 0;
      available = (const unsigned char *) memory.BaseAddress + memory.RegionSize - cursor;
      if (available >= bytes)
        return 1;
      cursor += available;
      bytes -= available;
    }

  return 1;
}

static unsigned char *
tap_context_register (CONTEXT *context)
{
  if (!strcmp (tap_register, "rax"))
    return (unsigned char *) (uintptr_t) context->Rax;
  if (!strcmp (tap_register, "rcx"))
    return (unsigned char *) (uintptr_t) context->Rcx;
  if (!strcmp (tap_register, "rdx"))
    return (unsigned char *) (uintptr_t) context->Rdx;
  if (!strcmp (tap_register, "r8"))
    return (unsigned char *) (uintptr_t) context->R8;
  if (!strcmp (tap_register, "r9"))
    return (unsigned char *) (uintptr_t) context->R9;
  if (!strcmp (tap_register, "r10"))
    return (unsigned char *) (uintptr_t) context->R10;
  if (!strcmp (tap_register, "r11"))
    return (unsigned char *) (uintptr_t) context->R11;
  if (!strcmp (tap_register, "r12"))
    return (unsigned char *) (uintptr_t) context->R12;
  if (!strcmp (tap_register, "r13"))
    return (unsigned char *) (uintptr_t) context->R13;
  if (!strcmp (tap_register, "r14"))
    return (unsigned char *) (uintptr_t) context->R14;
  if (!strcmp (tap_register, "r15"))
    return (unsigned char *) (uintptr_t) context->R15;
  if (!strcmp (tap_register, "rbx"))
    return (unsigned char *) (uintptr_t) context->Rbx;
  if (!strcmp (tap_register, "rsi"))
    return (unsigned char *) (uintptr_t) context->Rsi;
  if (!strcmp (tap_register, "rsp"))
    return (unsigned char *) (uintptr_t) context->Rsp;
  if (!strcmp (tap_register, "rbp"))
    return (unsigned char *) (uintptr_t) context->Rbp;
  return (unsigned char *) (uintptr_t) context->Rdi;
}

static LONG CALLBACK
tap_exception_handler (EXCEPTION_POINTERS *exception)
{
  CONTEXT *context = exception->ContextRecord;

  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == (void *) tap_address &&
      InterlockedCompareExchange (&tap_armed, 0, 0) == 0)
    {
      DWORD protection;
      unsigned char *source = tap_context_register (context);
      unsigned char register_value[sizeof (uintptr_t)];
      unsigned dereference;
      LONG occurrence = InterlockedIncrement (&tap_hits);

      VirtualProtect ((void *) tap_address, 1, PAGE_EXECUTE_READWRITE, &protection);
      *tap_address = tap_original_byte;
      FlushInstructionCache (GetCurrentProcess (), (void *) tap_address, 1);
      context->Rip = (DWORD64) (uintptr_t) tap_address;

      if ((unsigned) occurrence < tap_target_occurrence)
        {
          context->EFlags |= 0x100;
          InterlockedExchange (&tap_rearm, 1);
          return EXCEPTION_CONTINUE_EXECUTION;
        }

      InterlockedExchange (&tap_armed, 1);

      if (tap_module_source)
        source = tap_module_base + tap_module_rva;

      if (tap_register_value && !tap_module_source)
        {
          uintptr_t value = (uintptr_t) source;

          memcpy (register_value, &value, sizeof (value));
          source = register_value;
        }

      if (!tap_register_value && tap_rsp_offset)
        {
          source = (unsigned char *) (uintptr_t) context->Rsp + tap_rsp_offset;
          if (!tap_is_readable (source, sizeof (source)))
            source = NULL;
          else
            source = *(unsigned char **) source;
        }
      if (source && !tap_register_value)
        source += tap_pre_add_offset;
      for (dereference = 0; !tap_register_value && dereference < tap_derefs;
           dereference++)
        {
          if (!tap_is_readable (source, sizeof (source)))
            {
              source = NULL;
              break;
            }
          source = *(unsigned char **) source;
        }

      if (source && !tap_register_value)
        source += tap_add_offset;
      for (dereference = 0; !tap_register_value &&
           dereference < tap_post_derefs; dereference++)
        {
          if (!tap_is_readable (source, sizeof (source)))
            {
              source = NULL;
              break;
            }
          source = *(unsigned char **) source;
        }

      tap_pointer = (unsigned long long) (uintptr_t) source;
      if (!tap_is_readable (source, tap_bytes))
        InterlockedExchange (&tap_copy_failed, 1);
      else
        memcpy (tap_data, source, tap_bytes);
      return EXCEPTION_CONTINUE_EXECUTION;
    }

  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      InterlockedCompareExchange (&tap_rearm, 0, 1) == 1)
    {
      DWORD protection;

      VirtualProtect ((void *) tap_address, 1, PAGE_EXECUTE_READWRITE, &protection);
      *tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), (void *) tap_address, 1);
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }

  return EXCEPTION_CONTINUE_SEARCH;
}

static PVOID
tap_install (HMODULE chicago)
{
  const char *rva_string = getenv ("CHICAGO_TAP_RVA");
  const char *bytes_string;
  const char *rsp_offset_string;
  const char *pre_add_offset_string;
  const char *add_offset_string;
  const char *derefs_string;
  const char *post_derefs_string;
  const char *occurrence_string;
  const char *module_rva_string;
  DWORD protection;
  char *end;
  unsigned long rva;

  if (!rva_string || !*rva_string)
    return NULL;

  rva = strtoul (rva_string, &end, 16);
  if (*end || rva == 0)
    {
      fprintf (stderr, "invalid CHICAGO_TAP_RVA: %s\n", rva_string);
      return NULL;
    }

  tap_register = getenv ("CHICAGO_TAP_REG");
  if (!tap_register || (strcmp (tap_register, "rax") &&
                        strcmp (tap_register, "rcx") &&
                        strcmp (tap_register, "rdx") &&
                        strcmp (tap_register, "r8") &&
                        strcmp (tap_register, "r9") &&
                        strcmp (tap_register, "r10") &&
                        strcmp (tap_register, "r11") &&
                        strcmp (tap_register, "r12") &&
                        strcmp (tap_register, "r13") &&
                        strcmp (tap_register, "r14") &&
                        strcmp (tap_register, "r15") &&
                        strcmp (tap_register, "rbx") &&
                        strcmp (tap_register, "rdi") &&
                        strcmp (tap_register, "rsi") &&
                        strcmp (tap_register, "rsp") &&
                        strcmp (tap_register, "rbp")))
    tap_register = "r14";

  tap_bytes = RAW_BYTES;
  bytes_string = getenv ("CHICAGO_TAP_BYTES");
  if (bytes_string && *bytes_string)
    {
      tap_bytes = strtoul (bytes_string, &end, 0);
      if (*end || tap_bytes == 0 || tap_bytes > 0x100000)
        {
          fprintf (stderr, "invalid CHICAGO_TAP_BYTES: %s\n", bytes_string);
          return NULL;
        }
    }

  tap_rsp_offset = 0;
  rsp_offset_string = getenv ("CHICAGO_TAP_RSP_OFFSET");
  if (rsp_offset_string && *rsp_offset_string)
    {
      tap_rsp_offset = strtol (rsp_offset_string, &end, 0);
      if (*end)
        {
          fprintf (stderr, "invalid CHICAGO_TAP_RSP_OFFSET: %s\n", rsp_offset_string);
          return NULL;
        }
    }

  tap_pre_add_offset = 0;
  pre_add_offset_string = getenv ("CHICAGO_TAP_PRE_ADD_OFFSET");
  if (pre_add_offset_string && *pre_add_offset_string)
    {
      tap_pre_add_offset = strtol (pre_add_offset_string, &end, 0);
      if (*end)
        {
          fprintf (stderr, "invalid CHICAGO_TAP_PRE_ADD_OFFSET: %s\n",
                   pre_add_offset_string);
          return NULL;
        }
    }

  tap_derefs = 0;
  derefs_string = getenv ("CHICAGO_TAP_DEREFS");
  if (derefs_string && *derefs_string)
    {
      tap_derefs = strtoul (derefs_string, &end, 0);
      if (*end || tap_derefs > 3)
        {
          fprintf (stderr, "invalid CHICAGO_TAP_DEREFS: %s\n", derefs_string);
          return NULL;
        }
    }

  tap_add_offset = 0;
  add_offset_string = getenv ("CHICAGO_TAP_ADD_OFFSET");
  if (add_offset_string && *add_offset_string)
    {
      tap_add_offset = strtol (add_offset_string, &end, 0);
      if (*end)
        {
          fprintf (stderr, "invalid CHICAGO_TAP_ADD_OFFSET: %s\n", add_offset_string);
          return NULL;
        }
    }

  tap_post_derefs = 0;
  post_derefs_string = getenv ("CHICAGO_TAP_POST_DEREFS");
  if (post_derefs_string && *post_derefs_string)
    {
      tap_post_derefs = strtoul (post_derefs_string, &end, 0);
      if (*end || tap_post_derefs > 3)
        {
          fprintf (stderr, "invalid CHICAGO_TAP_POST_DEREFS: %s\n",
                   post_derefs_string);
          return NULL;
        }
    }

  tap_register_value = getenv ("CHICAGO_TAP_REGISTER_VALUE") != NULL;
  if (tap_register_value && tap_bytes > sizeof (uintptr_t))
    {
      fprintf (stderr, "register-value taps are limited to %zu bytes\n",
               sizeof (uintptr_t));
      return NULL;
    }

  tap_module_source = 0;
  tap_module_rva = 0;
  module_rva_string = getenv ("CHICAGO_TAP_MODULE_RVA");
  if (module_rva_string && *module_rva_string)
    {
      tap_module_rva = strtoul (module_rva_string, &end, 16);
      if (*end || tap_module_rva == 0)
        {
          fprintf (stderr, "invalid CHICAGO_TAP_MODULE_RVA: %s\n",
                   module_rva_string);
          return NULL;
        }
      tap_module_source = 1;
      tap_register_value = 0;
    }

  tap_target_occurrence = 1;
  occurrence_string = getenv ("CHICAGO_TAP_OCCURRENCE");
  if (occurrence_string && *occurrence_string)
    {
      tap_target_occurrence = strtoul (occurrence_string, &end, 0);
      if (*end || tap_target_occurrence == 0 || tap_target_occurrence > 1000)
        {
          fprintf (stderr, "invalid CHICAGO_TAP_OCCURRENCE: %s\n",
                   occurrence_string);
          return NULL;
        }
    }

  tap_data = calloc (1, tap_bytes);
  tap_module_base = (unsigned char *) chicago;
  tap_address = (unsigned char *) chicago + rva;
  if (!tap_data || !VirtualProtect ((void *) tap_address, 1,
                                     PAGE_EXECUTE_READWRITE, &protection))
    {
      fprintf (stderr, "could not install Chicago instruction tap\n");
      free (tap_data);
      tap_data = NULL;
      return NULL;
    }

  tap_original_byte = *tap_address;
  *tap_address = 0xcc;
  FlushInstructionCache (GetCurrentProcess (), (void *) tap_address, 1);
  tap_armed = 0;
  tap_hits = 0;
  tap_rearm = 0;
  tap_copy_failed = 0;
  return AddVectoredExceptionHandler (1, tap_exception_handler);
}

static void *
read_exact (const char *path,
            size_t      bytes)
{
  FILE *file = fopen (path, "rb");
  void *data = calloc (1, bytes);

  if (!file || !data || fread (data, 1, bytes, file) != bytes)
    {
      if (file)
        fclose (file);
      free (data);
      return NULL;
    }

  fclose (file);
  return data;
}

static int
write_exact (const char *path,
             const void *data,
             size_t      bytes)
{
  FILE *file = fopen (path, "wb");
  int ok = file && fwrite (data, 1, bytes, file) == bytes;

  if (file)
    fclose (file);
  return ok;
}

int
main (int   argc,
      char *argv[])
{
  const char *calibration_path;
  HMODULE chicago;
  ppp_param_init_t ppp_param_init;
  preprocess_init_calidata_t preprocess_init_calidata;
  preprocess_get_calidata_len_t preprocess_get_calidata_len;
  preprocess_load_calidata_t preprocess_load_calidata;
  preprocessor_init_t preprocessor_init;
  preprocess_set_mode_t preprocess_set_mode;
  preprocessor_t preprocessor;
  unsigned calibration_len;
  unsigned char *calibration_file;
  unsigned short *base;
  unsigned short *raw;
  unsigned char *enhanced;
  unsigned char *input;
  unsigned char *output;
  unsigned char *quality_context;
  void *scratch1;
  void *scratch2;
  void *config;
  PVOID tap_handler = NULL;
  const char *tap_output;
  const char *tap_phase;
  int ret;

  if (argc < 4 || argc > 5)
    {
      fprintf (stderr, "usage: %s <base.raw> <frame.raw> <enh.bin> [meta.bin]\n", argv[0]);
      return 2;
    }

  chicago = LoadLibraryA ("AlgoChicago.dll");
  ppp_param_init = EXPORT (chicago, ppp_param_init_t, "ppp_param_init");
  preprocess_init_calidata = EXPORT (chicago, preprocess_init_calidata_t,
                                      "preprocess_init_calidata");
  preprocess_get_calidata_len = EXPORT (chicago, preprocess_get_calidata_len_t,
                                         "preprocess_get_calidata_len");
  preprocess_load_calidata = EXPORT (chicago, preprocess_load_calidata_t,
                                      "preprocess_load_calidata");
  preprocessor_init = EXPORT (chicago, preprocessor_init_t, "preprocessor_init");
  preprocess_set_mode = EXPORT (chicago, preprocess_set_mode_t, "preprocess_set_mode");
  preprocessor = EXPORT (chicago, preprocessor_t, "preprocessor");
  if (!chicago || !ppp_param_init || !preprocess_init_calidata ||
      !preprocess_get_calidata_len || !preprocess_load_calidata ||
      !preprocessor_init || !preprocess_set_mode || !preprocessor)
    {
      fprintf (stderr, "missing AlgoChicago export\n");
      return 1;
    }

  calibration_path = getenv ("CHICAGO_CAL");
  if (!calibration_path || !*calibration_path)
    calibration_path = "Z:\\mnt\\win3\\ProgramData\\Goodix\\goodix_calib.dat";

  ppp_param_init (12);
  preprocess_init_calidata ();
  calibration_len = preprocess_get_calidata_len ();
  if (calibration_len != 0x224b0)
    {
      fprintf (stderr, "unexpected Chicago calibration payload: 0x%x\n", calibration_len);
      return 1;
    }

  calibration_file = read_exact (calibration_path, 16 + calibration_len);
  base = read_exact (argv[1], RAW_BYTES);
  raw = read_exact (argv[2], RAW_BYTES);
  enhanced = calloc (1, IMAGE_BYTES);
  input = calloc (1, HEADER_BYTES);
  output = calloc (1, HEADER_BYTES);
  quality_context = calloc (1, 0x40);
  scratch1 = calloc (1, 0x8000);
  scratch2 = calloc (1, 0x8000);
  config = calloc (1, HEADER_BYTES);
  if (!calibration_file || !base || !raw || !enhanced || !input || !output ||
      !quality_context || !scratch1 || !scratch2 || !config)
    {
      fprintf (stderr, "could not allocate/read oracle inputs\n");
      return 1;
    }

  ret = preprocess_load_calidata (calibration_file + 16, calibration_len);
  if (ret)
    {
      fprintf (stderr, "preprocess_load_calidata returned 0x%x\n", ret);
      return 1;
    }

  PTR (config, 0x18, base);
  U32 (config, 0x24, WIDTH);
  U32 (config, 0x28, HEIGHT);
  tap_output = getenv ("CHICAGO_TAP_OUT");
  tap_phase = getenv ("CHICAGO_TAP_PHASE");
  if (getenv ("CHICAGO_TAP_RVA") && tap_phase &&
      !strcmp (tap_phase, "init"))
    {
      if (!tap_output || !*tap_output)
        {
          fprintf (stderr, "CHICAGO_TAP_OUT is required with CHICAGO_TAP_RVA\n");
          return 1;
        }
      tap_handler = tap_install (chicago);
      if (!tap_handler)
        return 1;
    }
  ret = preprocessor_init (config);
  if (ret)
    {
      fprintf (stderr, "preprocessor_init returned 0x%x\n", ret);
      return 1;
    }
  preprocess_set_mode (1);

  PTR (input, 0x00, raw);
  U16 (input, 0x08, WIDTH);
  U16 (input, 0x0a, HEIGHT);
  U32 (input, 0x14, RAW_BYTES);
  U16 (input, 0x18, 1);
  PTR (output, 0x00, enhanced);
  U16 (output, 0x08, WIDTH);
  U16 (output, 0x0a, HEIGHT);
  U32 (output, 0x14, IMAGE_BYTES);

  if (getenv ("CHICAGO_TAP_RVA") && !tap_handler)
    {
      if (!tap_output || !*tap_output)
        {
          fprintf (stderr, "CHICAGO_TAP_OUT is required with CHICAGO_TAP_RVA\n");
          return 1;
        }
      tap_handler = tap_install (chicago);
      if (!tap_handler)
        return 1;
    }

  ret = preprocessor (input, scratch1, scratch2, output, quality_context, 0, 0);
  if (getenv ("CHICAGO_EXTRACT_FEATURES"))
    {
      enrol_start_t enrol_start =
        EXPORT (chicago, enrol_start_t, "enrolStart");
      enrol_add_image_t enrol_add_image =
        EXPORT (chicago, enrol_add_image_t, "enrolAddImage");
      void *enrol_context = enrol_start ? enrol_start () : NULL;
      void *enrol_scratch1 = calloc (1, 0x8000);
      void *enrol_scratch2 = calloc (1, 0x8000);
      void *enrol_metadata = calloc (1, 0x1000);
      int enrol_ret;

      if (!enrol_start || !enrol_add_image || !enrol_context ||
          !enrol_scratch1 || !enrol_scratch2 || !enrol_metadata)
        {
          fprintf (stderr, "could not initialize Chicago feature oracle\n");
          return 1;
        }
      U16 (output, 0x18, 1);
      *((unsigned char *) output + 0x0e) = 8;
      *((unsigned char *) output + 0x0f) = 1;
      enrol_ret = enrol_add_image (enrol_context, output, enrol_scratch1,
                                   enrol_scratch2, 0, enrol_metadata);
      printf ("enrolAddImage=0x%x\n", enrol_ret);
    }
  if (tap_handler)
    {
      RemoveVectoredExceptionHandler (tap_handler);
      if (!tap_armed)
        {
          fprintf (stderr, "Chicago tap at RVA 0x%tx was not reached\n",
                   tap_address - (unsigned char *) chicago);
          return 1;
        }
      if (tap_copy_failed)
        {
          fprintf (stderr, "Chicago tap source 0x%llx is not readable for %zu bytes\n",
                   tap_pointer, tap_bytes);
          return 1;
        }
      if (!write_exact (tap_output, tap_data, tap_bytes))
        {
          fprintf (stderr, "could not write Chicago tap %s\n", tap_output);
          return 1;
        }
      printf ("tap=rva:0x%tx occurrence:%ld reg:%s ptr:0x%llx bytes:%zu out:%s\n",
              tap_address - (unsigned char *) chicago,
              tap_hits, tap_register, tap_pointer, tap_bytes, tap_output);
    }
  if (!write_exact (argv[3], enhanced, IMAGE_BYTES))
    {
      fprintf (stderr, "could not write %s\n", argv[3]);
      return 1;
    }
  if (argc == 5 && !write_exact (argv[4], output, HEADER_BYTES))
    {
      fprintf (stderr, "could not write %s\n", argv[4]);
      return 1;
    }

  printf ("preprocessor=0x%x quality=%u coverage=%u status=%u,%u enhanced=%s\n",
          ret, output[0x28], output[0x29], output[0x2a], output[0x2b], argv[3]);
  return 0;
}
