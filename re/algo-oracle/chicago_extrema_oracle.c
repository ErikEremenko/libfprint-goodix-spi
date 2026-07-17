// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct offline oracle for AlgoChicago+0x167f0 scale-space extrema. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 64
#define HEIGHT 80
#define PIXELS (WIDTH * HEIGHT)
#define SCALES 9

typedef int (*extremum_t) (void **, int, int, int, int, int *);

typedef struct
{
  int x;
  int y;
  int scale;
  int response;
} Extremum;

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
  unsigned char objects[SCALES][0x40] = { 0, };
  void *object_pointers[SCALES];
  uint16_t *planes[SCALES];
  Extremum *records;
  unsigned count = 0;
  unsigned capacity = 6 * (WIDTH - 12) * (HEIGHT - 12);
  HMODULE chicago;
  extremum_t is_extremum;
  FILE *output;

  if (argc != SCALES + 2)
    {
      fprintf (stderr, "usage: %s scale-0 ... scale-8 output.bin\n", argv[0]);
      return 2;
    }

  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    {
      fprintf (stderr, "could not load AlgoChicago.dll\n");
      return 1;
    }
  is_extremum = (extremum_t) (void *)
    ((unsigned char *) chicago + 0x167f0);
  records = calloc (capacity, sizeof (*records));
  if (!records)
    return 1;

  for (int scale = 0; scale < SCALES; scale++)
    {
      planes[scale] = read_exact (argv[scale + 1], PIXELS * sizeof (uint16_t));
      if (!planes[scale])
        {
          fprintf (stderr, "could not read %s\n", argv[scale + 1]);
          return 1;
        }
      *(int *) (objects[scale] + 0x00) = WIDTH;
      *(int *) (objects[scale] + 0x04) = HEIGHT;
      *(void **) (objects[scale] + 0x18) = planes[scale];
      object_pointers[scale] = objects[scale];
    }

  for (int scale = 6; scale >= 1; scale--)
    for (int y = 6; y < HEIGHT - 6; y++)
      for (int x = 6; x < WIDTH - 6; x++)
        {
          int response;

          if (is_extremum (object_pointers, x, y, scale, 0x148, &response))
            records[count++] = (Extremum) { x, y, scale, response };
        }

  output = fopen (argv[SCALES + 1], "wb");
  if (!output || fwrite (&count, sizeof (count), 1, output) != 1 ||
      fwrite (records, sizeof (*records), count, output) != count)
    {
      fprintf (stderr, "could not write %s\n", argv[SCALES + 1]);
      return 1;
    }
  fclose (output);
  printf ("extrema=%u out=%s\n", count, argv[SCALES + 1]);
  return 0;
}
