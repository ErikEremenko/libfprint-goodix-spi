// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Differential oracle for the three-count Q8 aggregate assembled by
 * AlgoChicago+0x2a3e9..+0x2a43b before the +0x251d0 study filter. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int (*template_unpack_t) (void *, int, void *, void **);
typedef void (*template_delete_t) (void *);
typedef int32_t (*identify_score_t) (int32_t *, void *, void *, int32_t,
                                     int32_t, void *, void *);

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t count;
  uint32_t reserved;
} aggregate_header_t;

typedef struct
{
  int32_t count_zero;
  int32_t count_mixed;
  int32_t count_one;
  int32_t aggregate;
} aggregate_vector_t;

static unsigned char *count_site;
static unsigned char count_original;
static unsigned char *filter_site;
static unsigned char filter_original;
static aggregate_vector_t *current_vector;
static int captured;

static void
write_byte (unsigned char *address,
            unsigned char  value)
{
  DWORD protection;

  VirtualProtect (address, 1, PAGE_EXECUTE_READWRITE, &protection);
  *address = value;
  FlushInstructionCache (GetCurrentProcess (), address, 1);
  VirtualProtect (address, 1, protection, &protection);
}

static LONG CALLBACK
aggregate_handler (EXCEPTION_POINTERS *exception)
{
  CONTEXT *registers = exception->ContextRecord;
  void *address = exception->ExceptionRecord->ExceptionAddress;

  if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
    return EXCEPTION_CONTINUE_SEARCH;
  if (address == count_site || address == count_site + 1)
    {
      int32_t *frame = (int32_t *) (uintptr_t) registers->Rbp;

      frame[0x104 / 4] = current_vector->count_zero;
      frame[0x108 / 4] = current_vector->count_mixed;
      frame[0x10c / 4] = current_vector->count_one;
      write_byte (count_site, count_original);
      filter_original = *filter_site;
      write_byte (filter_site, 0xcc);
      registers->Rip = (DWORD64) (uintptr_t) count_site;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (address == filter_site || address == filter_site + 1)
    {
      current_vector->aggregate = *(const int32_t *)
        (uintptr_t) (registers->Rsp + 0x28);
      captured = 1;
      write_byte (filter_site, filter_original);
      registers->Rip = (DWORD64) (uintptr_t) filter_site;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  return EXCEPTION_CONTINUE_SEARCH;
}

static unsigned char *
read_file (const char *path,
           size_t     *size)
{
  FILE *file = fopen (path, "rb");
  unsigned char *contents;
  long length;

  if (!file || fseek (file, 0, SEEK_END) != 0 ||
      (length = ftell (file)) <= 0 || fseek (file, 0, SEEK_SET) != 0)
    return NULL;
  contents = malloc ((size_t) length);
  if (!contents || fread (contents, 1, (size_t) length, file) !=
                   (size_t) length)
    return NULL;
  fclose (file);
  *size = (size_t) length;
  return contents;
}

static uint32_t
next_random (uint32_t *state)
{
  uint32_t value = *state;

  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  *state = value;
  return value;
}

int
main (int   argc,
      char *argv[])
{
  aggregate_header_t header = { 0x38414743, 1, 128, 0 };
  aggregate_vector_t vectors[128] = { 0, };
  unsigned char *packed;
  size_t packed_size;
  HMODULE chicago;
  template_unpack_t unpack;
  template_delete_t delete_template;
  identify_score_t identify_score;
  void *gallery = NULL;
  unsigned char *gallery_inner;
  void *probe;
  FILE *output;
  uint32_t random_state = 0x51c02a3e;

  if (argc != 3)
    {
      fprintf (stderr, "usage: %s gallery.bin vectors.bin\n", argv[0]);
      return 2;
    }
  packed = read_file (argv[1], &packed_size);
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!packed || !chicago)
    return 3;
  unpack = (template_unpack_t) GetProcAddress (chicago, "templateUnPack");
  delete_template =
    (template_delete_t) GetProcAddress (chicago, "templateDelete");
  identify_score =
    (identify_score_t) ((unsigned char *) chicago + 0x290f0);
  if (!unpack || !delete_template ||
      unpack (packed, (int) packed_size, NULL, &gallery) != 0 || !gallery)
    return 4;
  gallery_inner = *(unsigned char **) gallery;
  probe = *(void **) (gallery_inner + 0x30);
  count_site = (unsigned char *) chicago + 0x2a3e9;
  filter_site = (unsigned char *) chicago + 0x251d0;
  AddVectoredExceptionHandler (1, aggregate_handler);

  vectors[1].count_zero = 1;
  vectors[2].count_one = 1;
  vectors[3] = (aggregate_vector_t) { 1, 1, 1, 0 };
  vectors[4] = (aggregate_vector_t) { 3, 0, 1, 0 };
  vectors[5] = (aggregate_vector_t) { 400, 480, 400, 0 };
  for (uint32_t index = 6; index < header.count; index++)
    {
      vectors[index].count_zero = next_random (&random_state) % 427;
      vectors[index].count_mixed = next_random (&random_state) % 427;
      vectors[index].count_one = next_random (&random_state) % 427;
    }
  for (uint32_t index = 0; index < header.count; index++)
    {
      unsigned char scratch[0x694] = { 0, };
      int32_t detail[9] = { 0, };
      int32_t score = 0;

      current_vector = &vectors[index];
      captured = 0;
      count_original = *count_site;
      write_byte (count_site, 0xcc);
      identify_score (&score, probe, gallery_inner, 0, 0, scratch, detail);
      if (!captured)
        {
          fprintf (stderr, "aggregate boundary not reached for vector %u\n",
                   index);
          return 5;
        }
    }
  output = fopen (argv[2], "wb");
  if (!output || fwrite (&header, sizeof (header), 1, output) != 1 ||
      fwrite (vectors, sizeof (vectors), 1, output) != 1 ||
      fclose (output) != 0)
    return 6;
  printf ("wrote %u official study-aggregate vectors\n", header.count);
  delete_template (gallery);
  free (packed);
  return 0;
}
