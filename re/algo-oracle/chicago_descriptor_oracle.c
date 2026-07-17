// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct offline oracle for AlgoChicago+0x15c30 mode-24 descriptors. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64
#define HEIGHT 80
#define PIXELS (WIDTH * HEIGHT)
#define FEATURE_BYTES 0x3c

typedef void (*descriptor_table_init_t) (void *);
typedef void (*descriptor_t) (void *, int, int, int, void *, void *, void *,
                              void *);

typedef struct
{
  int32_t strength;
  int32_t index;
} RankedCandidate;

typedef struct
{
  int32_t x;
  int32_t y;
  int32_t scale_value;
  uint32_t peak;
  uint32_t selected_peak;
  uint16_t secondary_orientation;
  uint16_t reserved;
} OrientationCandidate;

static void *
read_exact (const char *path,
            size_t      bytes)
{
  FILE *file = fopen (path, "rb");
  void *data = malloc (bytes);

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

static long
file_size (const char *path)
{
  FILE *file = fopen (path, "rb");
  long size;

  if (!file || fseek (file, 0, SEEK_END) != 0)
    return -1;
  size = ftell (file);
  fclose (file);
  return size;
}

int
main (int   argc,
      char *argv[])
{
  unsigned char magnitude_object[0x40] = { 0, };
  unsigned char orientation_object[0x40] = { 0, };
  const uint32_t config[15] = {
    24, 0x78, 0x12c, 0x28, 1, 0x1822e, 0x20, 4, 0,
    0x20, 1, 0, 0x78, 0x54, 0x62,
  };
  uint8_t *materialization;
  uint8_t *features;
  const OrientationCandidate *oriented;
  uint32_t *magnitude;
  int16_t *orientation;
  void *descriptor_table = calloc (1, 0x8000);
  uint32_t feature_count;
  long materialization_size;
  size_t expected_size;
  size_t oriented_offset;
  HMODULE chicago;
  descriptor_table_init_t initialize_table;
  descriptor_t build_descriptor;
  FILE *output;

  if (argc != 5)
    {
      fprintf (stderr, "usage: %s materialization.bin magnitude orientation "
                       "output.bin\n", argv[0]);
      return 2;
    }

  materialization_size = file_size (argv[1]);
  if (materialization_size < 8)
    {
      fprintf (stderr, "invalid materialization input\n");
      return 1;
    }
  materialization = read_exact (argv[1], materialization_size);
  magnitude = read_exact (argv[2], PIXELS * sizeof (uint32_t));
  orientation = read_exact (argv[3], PIXELS * sizeof (int16_t));
  if (!materialization || !magnitude || !orientation || !descriptor_table)
    {
      fprintf (stderr, "could not read descriptor inputs\n");
      return 1;
    }
  memcpy (&feature_count, materialization + 4, sizeof (feature_count));
  expected_size = 8 + feature_count *
    (FEATURE_BYTES + sizeof (RankedCandidate) + sizeof (OrientationCandidate));
  if ((size_t) materialization_size != expected_size)
    {
      fprintf (stderr, "invalid materialization size\n");
      return 1;
    }
  features = materialization + 8;
  oriented_offset = 8 + feature_count *
    (FEATURE_BYTES + sizeof (RankedCandidate));
  oriented = (const OrientationCandidate *) (materialization + oriented_offset);

  *(int *) (magnitude_object + 0x00) = WIDTH;
  *(int *) (magnitude_object + 0x04) = HEIGHT;
  *(void **) (magnitude_object + 0x18) = magnitude;
  *(int *) (orientation_object + 0x00) = WIDTH;
  *(int *) (orientation_object + 0x04) = HEIGHT;
  *(void **) (orientation_object + 0x18) = orientation;

  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    {
      fprintf (stderr, "could not load AlgoChicago.dll\n");
      return 1;
    }
  initialize_table = (descriptor_table_init_t) (void *)
    ((unsigned char *) chicago + 0x17d20);
  build_descriptor = (descriptor_t) (void *)
    ((unsigned char *) chicago + 0x15c30);
  initialize_table (descriptor_table);
  for (uint32_t index = 0; index < feature_count; index++)
    build_descriptor (features + index * FEATURE_BYTES,
                      oriented[index].x, oriented[index].y, config[5],
                      orientation_object, magnitude_object, descriptor_table,
                      (void *) config);

  output = fopen (argv[4], "wb");
  if (!output || fwrite (&feature_count, sizeof (feature_count), 1, output) != 1 ||
      fwrite (features, FEATURE_BYTES, feature_count, output) != feature_count)
    {
      fprintf (stderr, "could not write %s\n", argv[4]);
      return 1;
    }
  fclose (output);
  printf ("features=%u out=%s\n", feature_count, argv[4]);
  return 0;
}
