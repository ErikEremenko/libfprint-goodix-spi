// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct replay oracle for AlgoChicago+0x593f0 affine refinement. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*refine_t) (const int32_t *, const int32_t *, const uint8_t *,
                          int, int, int32_t *);

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

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t count;
  int32_t error_limit;
  int32_t source_xy[42 * 2];
  int32_t target_xy[42 * 2];
  uint8_t inliers[42];
  uint8_t padding[2];
  int32_t initial_transform[6];
  int32_t refined_transform[6];
} refinement_vector_t;

int
main (int argc,
      char **argv)
{
  geometry_vector_t geometry_vector;
  refinement_vector_t vector = { 0, };
  HMODULE chicago;
  refine_t refine;
  FILE *file;

  if (argc != 3)
    {
      fprintf (stderr, "usage: %s geometry-vector.bin output.bin\n", argv[0]);
      return 2;
    }
  file = fopen (argv[1], "rb");
  if (!file || fread (&geometry_vector, 1, sizeof (geometry_vector), file) !=
               sizeof (geometry_vector) || fclose (file) != 0 ||
      geometry_vector.magic != 0x4f454743 || geometry_vector.version != 1 ||
      geometry_vector.count > 42)
    return 3;
  vector.magic = 0x46455243;
  vector.version = 1;
  vector.count = geometry_vector.count;
  vector.error_limit = geometry_vector.error;
  {
    const char *drop_mod_text = getenv ("CHICAGO_REFINE_DROP_MOD");
    const char *error_text = getenv ("CHICAGO_REFINE_ERROR_LIMIT");

    if (drop_mod_text)
      {
        const int drop_mod = atoi (drop_mod_text);

        if (drop_mod < 2)
          return 6;
        for (uint32_t index = 0; index < geometry_vector.count; index++)
          if (index % (uint32_t) drop_mod == 0)
            geometry_vector.inliers[index] = 0;
      }
    if (error_text)
      vector.error_limit = atoi (error_text);
  }
  memcpy (vector.source_xy, geometry_vector.source_xy,
          sizeof (vector.source_xy));
  memcpy (vector.target_xy, geometry_vector.target_xy,
          sizeof (vector.target_xy));
  memcpy (vector.inliers, geometry_vector.inliers, sizeof (vector.inliers));
  memcpy (vector.initial_transform, geometry_vector.transform,
          sizeof (vector.initial_transform));
  memcpy (vector.refined_transform, geometry_vector.transform,
          sizeof (vector.refined_transform));
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 4;
  refine = (refine_t) ((unsigned char *) chicago + 0x593f0);
  refine (vector.source_xy, vector.target_xy, vector.inliers,
          (int) vector.count, vector.error_limit, vector.refined_transform);
  file = fopen (argv[2], "wb");
  if (!file || fwrite (&vector, 1, sizeof (vector), file) != sizeof (vector) ||
      fclose (file) != 0)
    return 5;
  printf ("count=%u error=%d initial=%d,%d,%d,%d,%d,%d "
          "refined=%d,%d,%d,%d,%d,%d\n",
          vector.count, vector.error_limit,
          vector.initial_transform[0], vector.initial_transform[1],
          vector.initial_transform[2], vector.initial_transform[3],
          vector.initial_transform[4], vector.initial_transform[5],
          vector.refined_transform[0], vector.refined_transform[1],
          vector.refined_transform[2], vector.refined_transform[3],
          vector.refined_transform[4], vector.refined_transform[5]);
  return 0;
}
