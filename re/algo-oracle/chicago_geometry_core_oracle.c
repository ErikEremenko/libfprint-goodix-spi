// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct replay oracle for AlgoChicago+0x58b20 geometry vectors. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*geometry_core_t) (const int32_t *, const int32_t *,
                                 const int32_t *, const int32_t *, int,
                                 int32_t *, uint8_t *, int32_t *, int, int);

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t count;
  uint32_t mode;
  uint32_t strict;
  int32_t transform[6];
  int32_t error;
  uint8_t inliers[42];
  uint8_t padding[2];
  int32_t source_xy[42 * 2];
  int32_t target_xy[42 * 2];
  int32_t source_orientation[42];
  int32_t target_orientation[42];
} geometry_vector_t;

int
main (int argc,
      char **argv)
{
  geometry_vector_t vector;
  HMODULE chicago;
  geometry_core_t geometry;
  FILE *file;

  if (argc != 3 && argc != 10)
    {
      fprintf (stderr, "usage: %s input.bin output.bin "
                       "[a b tx c d ty orientation-delta]\n", argv[0]);
      return 2;
    }
  file = fopen (argv[1], "rb");
  if (!file || fread (&vector, 1, sizeof (vector), file) != sizeof (vector) ||
      fclose (file) != 0 || vector.magic != 0x4f454743 ||
      vector.version != 1 || vector.count > 42)
    return 3;
  if (argc == 10)
    {
      const int32_t a = atoi (argv[3]);
      const int32_t b = atoi (argv[4]);
      const int32_t tx = atoi (argv[5]);
      const int32_t c = atoi (argv[6]);
      const int32_t d = atoi (argv[7]);
      const int32_t ty = atoi (argv[8]);
      const int32_t orientation_delta = atoi (argv[9]);

      for (uint32_t index = 0; index < vector.count; index++)
        {
          const int32_t x = vector.source_xy[index * 2];
          const int32_t y = vector.source_xy[index * 2 + 1];

          vector.target_xy[index * 2] =
            (int32_t) (((int64_t) a * x + (int64_t) b * y + 0x80) >> 8) + tx;
          vector.target_xy[index * 2 + 1] =
            (int32_t) (((int64_t) c * x + (int64_t) d * y + 0x80) >> 8) + ty;
          vector.target_orientation[index] =
            vector.source_orientation[index] + orientation_delta;
        }
    }
  memset (vector.transform, 0, sizeof (vector.transform));
  memset (vector.inliers, 0, sizeof (vector.inliers));
  vector.error = 0;
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 4;
  geometry = (geometry_core_t) ((unsigned char *) chicago + 0x58b20);
  geometry (vector.source_xy, vector.target_xy,
            vector.target_orientation, vector.source_orientation,
            (int) vector.count, vector.transform, vector.inliers,
            &vector.error, (int) vector.mode, (int) vector.strict);
  file = fopen (argv[2], "wb");
  if (!file || fwrite (&vector, 1, sizeof (vector), file) != sizeof (vector) ||
      fclose (file) != 0)
    return 5;
  printf ("count=%u error=%d transform=%d,%d,%d,%d,%d,%d\n",
          vector.count, vector.error,
          vector.transform[0], vector.transform[1], vector.transform[2],
          vector.transform[3], vector.transform[4], vector.transform[5]);
  return 0;
}
