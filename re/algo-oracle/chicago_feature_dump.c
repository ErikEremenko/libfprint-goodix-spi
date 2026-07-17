// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * Offline production Chicago enhanced-image feature oracle.
 *
 * The input image and metadata must come from the production AlgoChicago
 * preprocessor (or an exact native reproduction). This deliberately bypasses
 * raw preprocessing so the enhanced-image -> feature boundary can be reversed
 * and tested independently.
 *
 * Build:
 *   x86_64-w64-mingw32-gcc -O2 chicago_feature_dump.c -o chicago_feature_dump.exe
 * Run:
 *   wine chicago_feature_dump.exe base.raw frame.raw enhanced.bin metadata.bin features.bin
 *
 * features.bin is [u32 count][u32 quality][u32 coverage], followed by count
 * vendor feature records of 0x3c bytes each.
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64
#define HEIGHT 80
#define IMAGE_BYTES (WIDTH * HEIGHT)
#define RAW_BYTES (IMAGE_BYTES * 2)
#define IMAGE_HEADER_BYTES 0x40
#define FEATURE_BYTES 0x3c

#define PTR(p, off, value) (*(void **) ((unsigned char *) (p) + (off)) = (void *) (value))
#define U16(p, off, value) (*(uint16_t *) ((unsigned char *) (p) + (off)) = (uint16_t) (value))
#define U32(p, off, value) (*(uint32_t *) ((unsigned char *) (p) + (off)) = (uint32_t) (value))
#define U8(p, off, value) (*(uint8_t *) ((unsigned char *) (p) + (off)) = (uint8_t) (value))
#define EXPORT(module, type, name) ((type) (void *) GetProcAddress ((module), (name)))

typedef int (*ppp_param_init_t) (int);
typedef int (*preprocess_init_calidata_t) (void);
typedef unsigned (*preprocess_get_calidata_len_t) (void);
typedef int (*preprocess_load_calidata_t) (void *, unsigned);
typedef int (*preprocess_set_mode_t) (int);
typedef int (*preprocessor_init_t) (void *);
typedef int (*preprocessor_t) (void *, void *, void *, void *, void *, char, char);
typedef void *(*enrol_start_t) (void);
typedef int (*enrol_add_image_t) (void *, void *, void *, void *, char, void *);
typedef int (*enrol_get_template_t) (void *, void **);

static unsigned char *map_tap_address;
static unsigned char map_tap_original;
static unsigned char *map_tap_object;
static uint32_t map_tap_bytes;
static uint32_t map_tap_mode;
static unsigned char *study_tap_address;
static unsigned char study_tap_original;
static uint32_t study_tap_positive;
static uint32_t study_tap_negative;
static uint32_t study_tap_neutral;
static uint32_t study_tap_class;
static uint32_t study_tap_inactive;
static uint32_t study_tap_count;
static unsigned char *auxiliary_tap_address;
static unsigned char auxiliary_tap_original;
static int32_t auxiliary_tap_values[6];
static unsigned char auxiliary_tap_map[IMAGE_BYTES];
static uint32_t auxiliary_tap_count;

static LONG CALLBACK
map_tap_handler (EXCEPTION_POINTERS *exception)
{
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == auxiliary_tap_address)
    {
      CONTEXT *context = exception->ContextRecord;
      const int32_t *values = (const int32_t *) (uintptr_t) context->Rdx;
      DWORD protection;

      VirtualProtect (auxiliary_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *auxiliary_tap_address = auxiliary_tap_original;
      FlushInstructionCache (GetCurrentProcess (), auxiliary_tap_address, 1);
      context->Rip = (DWORD64) (uintptr_t) auxiliary_tap_address;
      if (context->Rcx)
        memcpy (auxiliary_tap_map,
                (const void *) (uintptr_t) context->Rcx,
                sizeof (auxiliary_tap_map));
      if (values)
        {
          memcpy (auxiliary_tap_values, values,
                  sizeof (auxiliary_tap_values));
          auxiliary_tap_count++;
        }
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == study_tap_address)
    {
      CONTEXT *context = exception->ContextRecord;
      unsigned char *object = (unsigned char *) (uintptr_t) context->R14;
      DWORD protection;

      VirtualProtect (study_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *study_tap_address = study_tap_original;
      FlushInstructionCache (GetCurrentProcess (), study_tap_address, 1);
      context->Rip = (DWORD64) (uintptr_t) study_tap_address;
      if (object)
        {
          study_tap_negative = *(uint32_t *) ((unsigned char *)
                                               (uintptr_t) context->Rsp + 0x40);
          study_tap_neutral = *(uint32_t *) ((unsigned char *)
                                              (uintptr_t) context->Rsp + 0x48);
          study_tap_positive = *(uint32_t *) (object + 0x158);
          study_tap_class = *(uint32_t *) (object + 0x15c);
          study_tap_inactive = *(uint32_t *) (object + 0x160);
          study_tap_count = *(uint32_t *) (object + 0xf0);
        }
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == map_tap_address)
    {
      CONTEXT *context = exception->ContextRecord;
      unsigned char *object = (unsigned char *) (uintptr_t) context->Rdx;
      DWORD protection;

      VirtualProtect (map_tap_address, 1, PAGE_EXECUTE_READWRITE, &protection);
      *map_tap_address = map_tap_original;
      FlushInstructionCache (GetCurrentProcess (), map_tap_address, 1);
      context->Rip = (DWORD64) (uintptr_t) map_tap_address;
      map_tap_mode = (uint32_t) context->Rcx;
      if (object)
        {
          memcpy (map_tap_object, object, IMAGE_HEADER_BYTES);
          map_tap_bytes = *(uint32_t *) (object + 0x0c);
          if (map_tap_bytes <= RAW_BYTES && *(void **) (object + 0x18))
            memcpy (map_tap_object + IMAGE_HEADER_BYTES,
                    *(void **) (object + 0x18), map_tap_bytes);
          else
            map_tap_bytes = 0;
        }
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  return EXCEPTION_CONTINUE_SEARCH;
}

static void *
read_file (const char *path,
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

static void
shift_enhanced (unsigned char *image,
                int            shift_x,
                int            shift_y)
{
  unsigned char shifted[IMAGE_BYTES] = { 0, };

  for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < WIDTH; x++)
      {
        const int source_x = x - shift_x;
        const int source_y = y - shift_y;

        if (source_x >= 0 && source_x < WIDTH &&
            source_y >= 0 && source_y < HEIGHT)
          shifted[y * WIDTH + x] = image[source_y * WIDTH + source_x];
      }
  memcpy (image, shifted, sizeof (shifted));
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
  preprocess_set_mode_t preprocess_set_mode;
  preprocessor_init_t preprocessor_init;
  preprocessor_t preprocessor;
  enrol_start_t enrol_start;
  enrol_add_image_t enrol_add_image;
  enrol_get_template_t enrol_get_template;
  unsigned calibration_len;
  unsigned char *calibration;
  unsigned char *image_base;
  unsigned char *raw;
  unsigned char *enhanced;
  unsigned char *metadata;
  unsigned char *image;
  unsigned char *config;
  unsigned char *preprocess_input;
  unsigned char *preprocess_output;
  unsigned char *preprocessed;
  void *preprocess_scratch1;
  void *preprocess_scratch2;
  void *quality_context;
  void *scratch1;
  void *scratch2;
  void *enrol_metadata;
  void *context;
  void *template_handle = NULL;
  unsigned char *inner;
  unsigned char *subtemplate;
  unsigned char *features;
  uint32_t header[3];
  int feature_count;
  int add_ret;
  int get_ret;
  FILE *output;

  if (argc != 6)
    {
      fprintf (stderr, "usage: %s <base.raw> <frame.raw> <enhanced.bin> <metadata.bin> <features.bin>\n",
               argv[0]);
      return 2;
    }

  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (chicago && (getenv ("CHICAGO_MAP_SOURCE_OUT") ||
                  getenv ("CHICAGO_STUDY_TAP") ||
                  getenv ("CHICAGO_AUXILIARY_TAP")))
    {
      DWORD protection;

      AddVectoredExceptionHandler (1, map_tap_handler);
      if (getenv ("CHICAGO_MAP_SOURCE_OUT"))
        {
          map_tap_object = calloc (1, IMAGE_HEADER_BYTES + RAW_BYTES);
          map_tap_address = (unsigned char *) chicago + 0x54ec0;
          VirtualProtect (map_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          map_tap_original = *map_tap_address;
          *map_tap_address = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), map_tap_address, 1);
        }
      if (getenv ("CHICAGO_STUDY_TAP"))
        {
          /* Immediately after +0x12400 has written +0x158/+0x15c/+0x160. */
          study_tap_address = (unsigned char *) chicago + 0x1272a;
          VirtualProtect (study_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          study_tap_original = *study_tap_address;
          *study_tap_address = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), study_tap_address, 1);
        }
      if (getenv ("CHICAGO_AUXILIARY_TAP"))
        {
          /* +0x137a0 constructs six int32 values at rsp+0xb0, then +0x539d0
           * truncates their low bytes into the subtemplate's +0x148 buffer. */
          auxiliary_tap_address = (unsigned char *) chicago + 0x13dea;
          VirtualProtect (auxiliary_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          auxiliary_tap_original = *auxiliary_tap_address;
          *auxiliary_tap_address = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), auxiliary_tap_address,
                                 1);
        }
    }
  ppp_param_init = EXPORT (chicago, ppp_param_init_t, "ppp_param_init");
  preprocess_init_calidata =
    EXPORT (chicago, preprocess_init_calidata_t, "preprocess_init_calidata");
  preprocess_get_calidata_len =
    EXPORT (chicago, preprocess_get_calidata_len_t,
            "preprocess_get_calidata_len");
  preprocess_load_calidata =
    EXPORT (chicago, preprocess_load_calidata_t, "preprocess_load_calidata");
  preprocess_set_mode =
    EXPORT (chicago, preprocess_set_mode_t, "preprocess_set_mode");
  preprocessor_init =
    EXPORT (chicago, preprocessor_init_t, "preprocessor_init");
  preprocessor = EXPORT (chicago, preprocessor_t, "preprocessor");
  enrol_start = EXPORT (chicago, enrol_start_t, "enrolStart");
  enrol_add_image = EXPORT (chicago, enrol_add_image_t, "enrolAddImage");
  enrol_get_template = EXPORT (chicago, enrol_get_template_t,
                               "enrolGetTemplate");
  if (!chicago || !ppp_param_init || !preprocess_init_calidata ||
      !preprocess_get_calidata_len || !preprocess_load_calidata ||
      !preprocess_set_mode || !preprocessor_init || !preprocessor || !enrol_start ||
      !enrol_add_image || !enrol_get_template)
    {
      fprintf (stderr, "missing AlgoChicago export\n");
      return 1;
    }

  calibration_path = getenv ("CHICAGO_CAL");
  if (!calibration_path || !*calibration_path)
    calibration_path = "Z:\\mnt\\win3\\ProgramData\\Goodix\\goodix_calib.dat";

  ppp_param_init (getenv ("PPP") ? atoi (getenv ("PPP")) : 12);
  preprocess_init_calidata ();
  calibration_len = preprocess_get_calidata_len ();
  calibration = read_file (calibration_path, 16 + calibration_len);
  image_base = read_file (argv[1], RAW_BYTES);
  raw = read_file (argv[2], RAW_BYTES);
  enhanced = read_file (argv[3], IMAGE_BYTES);
  metadata = read_file (argv[4], IMAGE_HEADER_BYTES);
  image = calloc (1, IMAGE_HEADER_BYTES);
  config = calloc (1, IMAGE_HEADER_BYTES);
  preprocess_input = calloc (1, IMAGE_HEADER_BYTES);
  preprocess_output = calloc (1, IMAGE_HEADER_BYTES);
  preprocessed = calloc (1, IMAGE_BYTES);
  preprocess_scratch1 = calloc (1, 0x8000);
  preprocess_scratch2 = calloc (1, 0x8000);
  quality_context = calloc (1, 0x40);
  scratch1 = calloc (1, 0x8000);
  scratch2 = calloc (1, 0x8000);
  enrol_metadata = calloc (1, 0x1000);
  if (!calibration || !image_base || !raw || !enhanced || !metadata || !image ||
      !config || !preprocess_input || !preprocess_output || !preprocessed ||
      !preprocess_scratch1 || !preprocess_scratch2 || !quality_context ||
      !scratch1 || !scratch2 || !enrol_metadata)
    {
      fprintf (stderr, "could not allocate/read oracle inputs\n");
      return 1;
    }
  if (preprocess_load_calidata (calibration + 16, calibration_len) != 0)
    {
      fprintf (stderr, "could not load Chicago calibration\n");
      return 1;
    }
  PTR (config, 0x18, image_base);
  U32 (config, 0x24, WIDTH);
  U32 (config, 0x28, HEIGHT);
  if (preprocessor_init (config) != 0)
    {
      fprintf (stderr, "could not initialize Chicago ImageBase\n");
      return 1;
    }
  preprocess_set_mode (1);
  PTR (preprocess_input, 0x00, raw);
  U16 (preprocess_input, 0x08, WIDTH);
  U16 (preprocess_input, 0x0a, HEIGHT);
  U32 (preprocess_input, 0x14, RAW_BYTES);
  U16 (preprocess_input, 0x18, 1);
  PTR (preprocess_output, 0x00, preprocessed);
  U16 (preprocess_output, 0x08, WIDTH);
  U16 (preprocess_output, 0x0a, HEIGHT);
  U32 (preprocess_output, 0x14, IMAGE_BYTES);
  if (preprocessor (preprocess_input, preprocess_scratch1, preprocess_scratch2,
                    preprocess_output, quality_context, 0, 0) != 0)
    {
      fprintf (stderr, "could not prime Chicago preprocessing state\n");
      return 1;
    }
  if (memcmp (preprocessed, enhanced, IMAGE_BYTES) != 0 &&
      !getenv ("CHICAGO_ACCEPT_ENHANCED_VECTOR"))
    {
      fprintf (stderr, "supplied enhanced image differs from AlgoChicago output\n");
      return 1;
    }
  if (getenv ("CHICAGO_SHIFT_X") || getenv ("CHICAGO_SHIFT_Y"))
    shift_enhanced (enhanced,
                    getenv ("CHICAGO_SHIFT_X") ?
                      atoi (getenv ("CHICAGO_SHIFT_X")) : 0,
                    getenv ("CHICAGO_SHIFT_Y") ?
                      atoi (getenv ("CHICAGO_SHIFT_Y")) : 0);
  if (getenv ("CHICAGO_SHIFTED_ENHANCED_OUT"))
    {
      FILE *shifted_output =
        fopen (getenv ("CHICAGO_SHIFTED_ENHANCED_OUT"), "wb");

      if (!shifted_output ||
          fwrite (enhanced, 1, IMAGE_BYTES, shifted_output) != IMAGE_BYTES)
        return 1;
      fclose (shifted_output);
    }

  PTR (image, 0x00, enhanced);
  U16 (image, 0x08, getenv ("CHICAGO_FEATURE_GEOMETRY_80X64") ? 80 : WIDTH);
  U16 (image, 0x0a, getenv ("CHICAGO_FEATURE_GEOMETRY_80X64") ? 64 : HEIGHT);
  U8 (image, 0x0e, 8);
  U8 (image, 0x0f, 1);
  U32 (image, 0x14, IMAGE_BYTES);
  U16 (image, 0x18, 1);
  U8 (image, 0x28, metadata[0x28]);
  U8 (image, 0x29, metadata[0x29]);

  context = enrol_start ();
  add_ret = enrol_add_image (context, image, scratch1, scratch2, 0,
                             enrol_metadata);
  if (getenv ("CHICAGO_AUXILIARY_TAP"))
    {
      const char *map_path = getenv ("CHICAGO_AUXILIARY_MAP_OUT");
      unsigned nonzero = 0;

      for (unsigned index = 0; index < IMAGE_BYTES / 4; index++)
        nonzero += auxiliary_tap_map[index] != 0;
      printf ("live-auxiliary count=%u values=%d,%d,%d,%d,%d,%d bytes=%u,%u,%u,%u,%u,%u map-nonzero=%u\n",
            auxiliary_tap_count,
            auxiliary_tap_values[0], auxiliary_tap_values[1],
            auxiliary_tap_values[2], auxiliary_tap_values[3],
            auxiliary_tap_values[4], auxiliary_tap_values[5],
            (unsigned) (uint8_t) auxiliary_tap_values[0],
            (unsigned) (uint8_t) auxiliary_tap_values[1],
            (unsigned) (uint8_t) auxiliary_tap_values[2],
            (unsigned) (uint8_t) auxiliary_tap_values[3],
            (unsigned) (uint8_t) auxiliary_tap_values[4],
            (unsigned) (uint8_t) auxiliary_tap_values[5], nonzero);
      if (map_path && *map_path)
        {
          FILE *map_output = fopen (map_path, "wb");

          if (!map_output ||
              fwrite (auxiliary_tap_map, 1, IMAGE_BYTES / 4, map_output) !=
                IMAGE_BYTES / 4)
            return 1;
          fclose (map_output);
        }
    }
  if (getenv ("CHICAGO_STUDY_TAP"))
    printf ("live-density count=%u percentages=%u,%u,%u class=%u inactive=%u\n",
            study_tap_count, study_tap_negative, study_tap_positive,
            study_tap_neutral, study_tap_class, study_tap_inactive);
  get_ret = enrol_get_template (context, &template_handle);
  inner = template_handle ? *(unsigned char **) template_handle : NULL;
  /* The gallery object stores capacity at +0x28 and begins its subtemplate
   * pointer table at +0x30. The older Milan +0x28 pointer assumption is not
   * valid for this production Chicago build. */
  subtemplate = inner ? *(unsigned char **) (inner + 0x30) : NULL;
  feature_count = subtemplate ? *(int *) (subtemplate + 0xf0) : 0;
  features = subtemplate ? *(unsigned char **) (subtemplate + 0xf8) : NULL;
  printf ("context=%p samples=%u template=%p inner=%p inner+0x1c=%u sub=%p "
          "sub+0xf0=%d sub+0xf8=%p required=%u progress=%u position=%u,%u "
          "inner-count=%u inner-capacity=%u transforms=%u active=%u "
          "sub-quality=%u sub-coverage=%u density-positive=%u "
          "density-class=%u density-inactive=%u\n",
          context, context ? *(unsigned short *) ((unsigned char *) context + 0x0a) : 0,
          template_handle, inner, inner ? *(unsigned *) (inner + 0x1c) : 0,
          subtemplate, feature_count, features,
          context ? *(unsigned short *) ((unsigned char *) context + 0x08) : 0,
          context ? *(unsigned *) ((unsigned char *) context + 0x0c) : 0,
          context ? *(unsigned *) ((unsigned char *) context + 0x10) : 0,
          context ? *(unsigned *) ((unsigned char *) context + 0x14) : 0,
          inner ? *(unsigned *) (inner + 0x24) : 0,
          inner ? *(unsigned *) (inner + 0x28) : 0,
          inner ? *(unsigned *) (inner + 0x2c) : 0,
          subtemplate ? *(unsigned *) (subtemplate + 0x108) : 0,
          subtemplate ? *(unsigned *) (subtemplate + 0x10c) : 0,
          subtemplate ? *(unsigned *) (subtemplate + 0x110) : 0,
          subtemplate ? *(unsigned *) (subtemplate + 0x158) : 0,
          subtemplate ? *(unsigned *) (subtemplate + 0x15c) : 0,
          subtemplate ? *(unsigned *) (subtemplate + 0x160) : 0);
  if (inner && getenv ("CHICAGO_FEATURE_OBJECT_OUT"))
    {
      FILE *object_output = fopen (getenv ("CHICAGO_FEATURE_OBJECT_OUT"), "wb");

      if (object_output)
        {
          fwrite (inner, 1, 0x400, object_output);
          fclose (object_output);
        }
    }
  if (feature_count < 0 || feature_count > 10000 ||
      (feature_count != 0 && !features))
    {
      fprintf (stderr, "invalid Chicago feature result: %d\n", feature_count);
      return 1;
    }

  header[0] = feature_count;
  header[1] = metadata[0x28];
  header[2] = metadata[0x29];
  output = fopen (argv[5], "wb");
  if (!output || fwrite (header, sizeof (header), 1, output) != 1 ||
      (feature_count > 0 &&
       fwrite (features, FEATURE_BYTES, feature_count, output) !=
       (size_t) feature_count))
    {
      if (output)
        fclose (output);
      fprintf (stderr, "could not write %s\n", argv[5]);
      return 1;
    }
  fclose (output);

  printf ("add=0x%x get=0x%x features=%d quality=%u coverage=%u out=%s\n",
          add_ret, get_ret, feature_count, metadata[0x28], metadata[0x29],
          argv[5]);
  if (subtemplate && getenv ("CHICAGO_SUBTEMPLATE_INSPECT"))
    {
      for (size_t offset = 0; offset < 0xf0; offset += 8)
        {
          uintptr_t value;
          MEMORY_BASIC_INFORMATION memory;

          memcpy (&value, subtemplate + offset, sizeof (value));
          printf ("sub+%03x=%016llx", (unsigned) offset,
                  (unsigned long long) value);
          if (value > 0x10000 &&
              VirtualQuery ((const void *) value, &memory, sizeof (memory)) ==
                sizeof (memory) &&
              memory.State == MEM_COMMIT &&
              !(memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            {
              const unsigned char *bytes = (const unsigned char *) value;

              printf (" ->");
              for (size_t index = 0; index < 32; index++)
                printf ("%02x", bytes[index]);
            }
          printf ("\n");
        }
    }
  if (subtemplate && getenv ("CHICAGO_SUBTEMPLATE_OUT"))
    {
      FILE *subtemplate_output = fopen (getenv ("CHICAGO_SUBTEMPLATE_OUT"),
                                        "wb");
      const uint32_t magic = 0x4a424f53; /* SOBJ */

      if (!subtemplate_output)
        return 1;
      fwrite (&magic, sizeof (magic), 1, subtemplate_output);
      fwrite (subtemplate, 1, 0x160, subtemplate_output);
      for (size_t offset = 8; offset <= 0x18; offset += 8)
        {
          unsigned char *object = *(unsigned char **) (subtemplate + offset);
          uint32_t bytes = object ?
            *(uint32_t *) object * *(uint32_t *) (object + 4) : 0;

          fwrite (&bytes, sizeof (bytes), 1, subtemplate_output);
          if (object)
            {
              fwrite (object, 1, 32, subtemplate_output);
              fwrite (*(void **) (object + 24), 1, bytes,
                      subtemplate_output);
            }
        }
      fclose (subtemplate_output);
    }
  if (subtemplate && getenv ("CHICAGO_POSITION_MAP_OUT"))
    {
      const unsigned char *object =
        *(const unsigned char * const *) (subtemplate + 0x130);
      FILE *map_output = fopen (getenv ("CHICAGO_POSITION_MAP_OUT"), "wb");
      const uint32_t magic = 0x424f4d50; /* PMOB */
      const uint32_t bytes = object ? *(const uint32_t *) (object + 0x0c) : 0;

      if (!map_output || !object || !*(const void * const *) (object + 0x18))
        return 1;
      fwrite (&magic, sizeof (magic), 1, map_output);
      fwrite (object, 1, 32, map_output);
      fwrite (*(const void * const *) (object + 0x18), 1, bytes, map_output);
      fclose (map_output);
    }
  if (map_tap_object && getenv ("CHICAGO_MAP_SOURCE_OUT"))
    {
      FILE *map_output = fopen (getenv ("CHICAGO_MAP_SOURCE_OUT"), "wb");
      const uint32_t magic = 0x4352534d; /* MSRC */

      if (!map_output || map_tap_bytes == 0)
        return 1;
      fwrite (&magic, sizeof (magic), 1, map_output);
      fwrite (&map_tap_mode, sizeof (map_tap_mode), 1, map_output);
      fwrite (&map_tap_bytes, sizeof (map_tap_bytes), 1, map_output);
      fwrite (map_tap_object, 1, IMAGE_HEADER_BYTES + map_tap_bytes,
              map_output);
      fclose (map_output);
    }
  return 0;
}
