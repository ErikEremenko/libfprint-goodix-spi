// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct offline oracle for AlgoChicago+0x149c0 mode-24 feature records. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 64
#define HEIGHT 80
#define PIXELS (WIDTH * HEIGHT)
#define SCALES 9
#define CAPACITY 600
#define FEATURE_BYTES 0x3c

typedef int (*extremum_t) (void **, int, int, int, int, int *);
typedef void (*collect_t) (void *, void *, void *, void *, uint32_t *, void **,
                           void *, void *, void *, int, int, uint32_t, void *);

typedef struct
{
  int32_t x;
  int32_t y;
  int32_t scale;
  int32_t strength;
  int16_t refined_x;
  int16_t refined_y;
  int32_t scale_value;
} Candidate;

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

int
main (int   argc,
      char *argv[])
{
  unsigned char scale_objects[SCALES][0x40] = { 0, };
  unsigned char image_object[0x40] = { 0, };
  unsigned char magnitude_object[0x40] = { 0, };
  unsigned char orientation_object[0x40] = { 0, };
  void *scale_pointers[SCALES];
  uint16_t *planes[SCALES];
  uint8_t *feature_source;
  uint32_t *magnitude;
  int16_t *orientation;
  uint8_t *features = calloc (CAPACITY, FEATURE_BYTES);
  RankedCandidate *ranked = calloc (CAPACITY, sizeof (*ranked));
  OrientationCandidate *oriented = calloc (CAPACITY, sizeof (*oriented));
  uint8_t *visited = calloc (PIXELS, 1);
  uint32_t config[15] = {
    24, 0x78, 0x12c, 0x28, 1, 0x1822e, 0x20, 4, 0,
    0x20, 1, 0, 0x78, 0x54, 0x62,
  };
  uint32_t raw_count = 0;
  uint32_t feature_count = 0;
  HMODULE chicago;
  extremum_t is_extremum;
  collect_t collect;
  FILE *output;

  if (argc != SCALES + 5)
    {
      fprintf (stderr, "usage: %s scale-0 ... scale-8 feature-source magnitude "
                       "orientation output.bin\n", argv[0]);
      return 2;
    }
  if (!features || !ranked || !oriented || !visited)
    return 1;

  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    {
      fprintf (stderr, "could not load AlgoChicago.dll\n");
      return 1;
    }
  is_extremum = (extremum_t) (void *)
    ((unsigned char *) chicago + 0x167f0);
  collect = (collect_t) (void *) ((unsigned char *) chicago + 0x15810);

  for (int scale = 0; scale < SCALES; scale++)
    {
      planes[scale] = read_exact (argv[scale + 1],
                                  PIXELS * sizeof (uint16_t));
      if (!planes[scale])
        {
          fprintf (stderr, "could not read %s\n", argv[scale + 1]);
          return 1;
        }
      *(int *) (scale_objects[scale] + 0x00) = WIDTH;
      *(int *) (scale_objects[scale] + 0x04) = HEIGHT;
      *(void **) (scale_objects[scale] + 0x18) = planes[scale];
      scale_pointers[scale] = scale_objects[scale];
    }

  feature_source = read_exact (argv[10], PIXELS);
  magnitude = read_exact (argv[11], PIXELS * sizeof (uint32_t));
  orientation = read_exact (argv[12], PIXELS * sizeof (int16_t));
  if (!feature_source || !magnitude || !orientation)
    {
      fprintf (stderr, "could not read materialization inputs\n");
      return 1;
    }
  *(int *) (magnitude_object + 0x00) = WIDTH;
  *(int *) (magnitude_object + 0x04) = HEIGHT;
  *(void **) (magnitude_object + 0x18) = magnitude;
  *(int *) (orientation_object + 0x00) = WIDTH;
  *(int *) (orientation_object + 0x04) = HEIGHT;
  *(void **) (orientation_object + 0x18) = orientation;

  *(int *) (image_object + 0x00) = WIDTH;
  *(int *) (image_object + 0x04) = HEIGHT;
  *(void **) (image_object + 0x18) = feature_source;

  for (int scale = 6; scale >= 1; scale--)
    for (int y = 6; y < HEIGHT - 6; y++)
      for (int x = 6; x < WIDTH - 6; x++)
        {
          int response;

          if (!is_extremum (scale_pointers, x, y, scale, 0x148, &response))
            continue;
          raw_count++;
        }

  collect (image_object, features, ranked, oriented, &feature_count,
           scale_pointers, visited, magnitude_object, orientation_object,
           config[3], config[2], config[0], config);

  output = fopen (argv[13], "wb");
  if (!output || fwrite (&raw_count, sizeof (raw_count), 1, output) != 1 ||
      fwrite (&feature_count, sizeof (feature_count), 1, output) != 1 ||
      fwrite (features, FEATURE_BYTES, feature_count, output) != feature_count ||
      fwrite (ranked, sizeof (*ranked), feature_count, output) != feature_count ||
      fwrite (oriented, sizeof (*oriented), feature_count, output) != feature_count)
    {
      fprintf (stderr, "could not write %s\n", argv[13]);
      return 1;
    }
  fclose (output);
  printf ("extrema=%u features=%u out=%s\n", raw_count, feature_count,
          argv[13]);
  return 0;
}
