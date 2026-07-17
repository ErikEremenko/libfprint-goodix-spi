// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct offline oracle for AlgoChicago+0x510e0 relation metrics. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUBTEMPLATE_BYTES 0x160
#define IMAGE_OBJECT_BYTES 32

typedef int32_t (*metric_t) (void *, void *, const int32_t *, const int32_t *,
                             int32_t *, int32_t *, int32_t *, int32_t *,
                             int32_t, int32_t *, int32_t *);
typedef void *(*expand_mask_t) (const void *, int32_t, int32_t, uint32_t);
typedef void *(*unpack_bits_t) (const void *);
typedef void (*warp_region_t) (void *, void *, const int32_t *, void **,
                               void **, int32_t *);
typedef void (*compare_maps_t) (void *, void *, void *, void *, int32_t,
                                int32_t, int32_t *, int32_t *, int32_t *);

static unsigned char *compare_tap_address;
static unsigned char compare_tap_original;
static unsigned char *compare_return_address;
static unsigned char compare_return_original;
static int compare_tap_rearm;
static int compare_tap_enabled;
static int compare_call_index;
static int32_t *compare_counts;
static int32_t *compare_total;
static int32_t compare_dimensions[8];
static int32_t compare_offsets[2];

static LONG CALLBACK
compare_tap_handler (EXCEPTION_POINTERS *exception)
{
  CONTEXT *registers = exception->ContextRecord;
  DWORD protection;

  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == compare_return_address)
    {
      printf ("compare[%d] a=%d,%d b=%d,%d c=%d,%d d=%d,%d "
              "offset=%d,%d counts=%d,%d,%d,%d total=%d\n",
              compare_call_index++, compare_dimensions[0],
              compare_dimensions[1], compare_dimensions[2],
              compare_dimensions[3], compare_dimensions[4],
              compare_dimensions[5], compare_dimensions[6],
              compare_dimensions[7], compare_offsets[0], compare_offsets[1],
              compare_counts ? compare_counts[0] : -1,
              compare_counts ? compare_counts[1] : -1,
              compare_counts ? compare_counts[2] : -1,
              compare_counts ? compare_counts[3] : -1,
              compare_total ? *compare_total : -1);
      VirtualProtect (compare_return_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *compare_return_address = compare_return_original;
      FlushInstructionCache (GetCurrentProcess (), compare_return_address, 1);
      registers->Rip = (DWORD64) (uintptr_t) compare_return_address;
      compare_return_address = NULL;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == compare_tap_address)
    {
      const unsigned char *objects[4] = {
        (const unsigned char *) (uintptr_t) registers->Rcx,
        (const unsigned char *) (uintptr_t) registers->Rdx,
        (const unsigned char *) (uintptr_t) registers->R8,
        (const unsigned char *) (uintptr_t) registers->R9,
      };

      for (int index = 0; index < 4; index++)
        {
          compare_dimensions[index * 2] = objects[index] ?
            *(const int32_t *) objects[index] : -1;
          compare_dimensions[index * 2 + 1] = objects[index] ?
            *(const int32_t *) (objects[index] + 4) : -1;
        }
      if (getenv ("CHICAGO_COMPARE_DUMP"))
        {
          char path[MAX_PATH];
          FILE *dump;

          snprintf (path, sizeof (path), "%s-%d.bin",
                    getenv ("CHICAGO_COMPARE_DUMP"), compare_call_index);
          dump = fopen (path, "wb");
          if (dump)
            {
              for (int index = 0; index < 4; index++)
                {
                  const uint32_t bytes = objects[index] ?
                    *(const uint32_t *) (objects[index] + 12) : 0;
                  fwrite (objects[index], 1, 32, dump);
                  if (bytes && *(const void * const *) (objects[index] + 24))
                    fwrite (*(const void * const *) (objects[index] + 24),
                            1, bytes, dump);
                }
              fclose (dump);
            }
        }
      compare_offsets[0] = *(int32_t *) (uintptr_t) (registers->Rsp + 0x28);
      compare_offsets[1] = *(int32_t *) (uintptr_t) (registers->Rsp + 0x30);
      compare_counts = *(int32_t **) (uintptr_t) (registers->Rsp + 0x38);
      compare_total = *(int32_t **) (uintptr_t) (registers->Rsp + 0x40);
      compare_return_address =
        *(unsigned char **) (uintptr_t) registers->Rsp;
      VirtualProtect (compare_return_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      compare_return_original = *compare_return_address;
      *compare_return_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), compare_return_address, 1);
      VirtualProtect (compare_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *compare_tap_address = compare_tap_original;
      FlushInstructionCache (GetCurrentProcess (), compare_tap_address, 1);
      registers->Rip = (DWORD64) (uintptr_t) compare_tap_address;
      registers->EFlags |= 0x100;
      compare_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      compare_tap_rearm)
    {
      compare_tap_rearm = 0;
      registers->EFlags &= ~0x100;
      if (compare_tap_enabled)
        {
          VirtualProtect (compare_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          *compare_tap_address = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), compare_tap_address, 1);
        }
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  return EXCEPTION_CONTINUE_SEARCH;
}

typedef struct
{
  uint8_t object[IMAGE_OBJECT_BYTES];
  uint8_t *data;
} Image;

typedef struct
{
  uint8_t object[SUBTEMPLATE_BYTES];
  Image images[3];
} Subtemplate;

static int
read_subtemplate (const char *path,
                  Subtemplate *subtemplate)
{
  FILE *file = fopen (path, "rb");
  uint32_t magic;

  if (!file || fread (&magic, sizeof (magic), 1, file) != 1 ||
      magic != 0x4a424f53 ||
      fread (subtemplate->object, 1, SUBTEMPLATE_BYTES, file) !=
        SUBTEMPLATE_BYTES)
    return 0;
  for (size_t index = 0; index < 3; index++)
    {
      uint32_t bytes;

      if (fread (&bytes, sizeof (bytes), 1, file) != 1 ||
          fread (subtemplate->images[index].object, 1, IMAGE_OBJECT_BYTES,
                 file) != IMAGE_OBJECT_BYTES)
        return 0;
      subtemplate->images[index].data = malloc (bytes);
      if ((bytes != 0 && !subtemplate->images[index].data) ||
          fread (subtemplate->images[index].data, 1, bytes, file) != bytes)
        return 0;
      *(void **) (subtemplate->images[index].object + 24) =
        subtemplate->images[index].data;
      *(void **) (subtemplate->object + 8 + index * 8) =
        subtemplate->images[index].object;
    }
  fclose (file);
  return 1;
}

static int
read_transform (const char *path,
                int32_t     transform[6])
{
  const char *values = getenv ("CHICAGO_TRANSFORM_VALUES");
  FILE *file = fopen (path, "rb");
  uint32_t count;

  if (values && sscanf (values, "%d,%d,%d,%d,%d,%d",
                        &transform[0], &transform[1], &transform[2],
                        &transform[3], &transform[4], &transform[5]) == 6)
    return 1;

  if (!file || fread (&count, sizeof (count), 1, file) != 1 ||
      fread (transform, sizeof (*transform), 6, file) != 6)
    return 0;
  fclose (file);
  return 1;
}

int
main (int argc,
      char *argv[])
{
  Subtemplate old_subtemplate = { 0, };
  Subtemplate new_subtemplate = { 0, };
  int32_t transform[6];
  int32_t config[3] = { 0, 4, 0 };
  int32_t output5 = 0;
  int32_t output6 = 0;
  int32_t metric_a = 0;
  int32_t metric_b = 0;
  int32_t geometry_count = 0;
  int32_t metric_return;
  HMODULE chicago;
  metric_t calculate_metrics;
  expand_mask_t expand_mask;
  unpack_bits_t unpack_bits;
  warp_region_t warp_region;
  compare_maps_t compare_maps;
  void *new_plane;
  void *old_plane;
  void *new_expanded_mask;
  void *old_expanded_mask;
  void *warped_plane = NULL;
  void *warped_mask = NULL;
  int32_t bounds[9] = { 40, 32, 0, 40, 0, 4, 0, 0, 0 };
  int32_t counts[4] = { 0, };
  int32_t compared = 0;
  FILE *output;

  if (argc != 5)
    {
      fprintf (stderr, "usage: %s old.sobj new.sobj transform.bin out.bin\n",
               argv[0]);
      return 2;
    }
  if (getenv ("CHICAGO_CONFIG"))
    sscanf (getenv ("CHICAGO_CONFIG"), "%d,%d,%d",
            &config[0], &config[1], &config[2]);
  if (getenv ("CHICAGO_GEOMETRY_COUNT"))
    geometry_count = atoi (getenv ("CHICAGO_GEOMETRY_COUNT"));
  if (!read_subtemplate (argv[1], &old_subtemplate) ||
      !read_subtemplate (argv[2], &new_subtemplate) ||
      !read_transform (argv[3], transform))
    {
      fprintf (stderr, "could not read metric inputs\n");
      return 1;
    }
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 1;
  calculate_metrics = (metric_t) (void *)
    ((unsigned char *) chicago + 0x510e0);
  expand_mask = (expand_mask_t) (void *)
    ((unsigned char *) chicago + 0x519d0);
  unpack_bits = (unpack_bits_t) (void *)
    ((unsigned char *) chicago + 0x52cf0);
  warp_region = (warp_region_t) (void *)
    ((unsigned char *) chicago + 0x51670);
  compare_maps = (compare_maps_t) (void *)
    ((unsigned char *) chicago + 0x546b0);
  if (getenv ("CHICAGO_TRACE_COMPARE"))
    {
      DWORD protection;

      compare_tap_address = (unsigned char *) chicago + 0x546b0;
      compare_tap_original = *compare_tap_address;
      compare_tap_enabled = 1;
      AddVectoredExceptionHandler (1, compare_tap_handler);
      VirtualProtect (compare_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *compare_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), compare_tap_address, 1);
    }
  metric_return = calculate_metrics (
    new_subtemplate.object, old_subtemplate.object, transform, config,
    &output5, &output6, &metric_a, &metric_b, geometry_count, NULL, NULL);
  if (compare_tap_enabled)
    {
      DWORD protection;

      compare_tap_enabled = 0;
      VirtualProtect (compare_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *compare_tap_address = compare_tap_original;
      FlushInstructionCache (GetCurrentProcess (), compare_tap_address, 1);
    }

  new_plane = unpack_bits (new_subtemplate.images[0].object);
  old_plane = unpack_bits (old_subtemplate.images[0].object);
  new_expanded_mask = expand_mask (new_subtemplate.object + 0x28, 0, 80, 64);
  old_expanded_mask = expand_mask (old_subtemplate.object + 0x28, 0, 80, 64);
  warp_region (new_plane, new_expanded_mask, transform, &warped_plane,
               &warped_mask, bounds);
  if (warped_plane && warped_mask)
    compare_maps (old_plane, warped_plane, old_expanded_mask, warped_mask,
                  bounds[7], bounds[6], counts, &compared, NULL);

  output = fopen (argv[4], "wb");
  if (!output || fwrite (&metric_a, sizeof (metric_a), 1, output) != 1 ||
      fwrite (&metric_b, sizeof (metric_b), 1, output) != 1)
    return 1;
  fclose (output);
  printf ("metric_return=%d outputs=%d,%d,%d,%d geometry=%d\n",
          metric_return, output5, output6, metric_a, metric_b,
          geometry_count);
  printf ("bounds=%d,%d,%d,%d,%d,%d,%d,%d,%d counts=%d,%d,%d,%d total=%d\n",
          bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5],
          bounds[6], bounds[7], bounds[8], counts[0], counts[1], counts[2],
          counts[3], compared);
  return 0;
}
