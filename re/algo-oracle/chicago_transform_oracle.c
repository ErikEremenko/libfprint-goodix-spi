// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct offline oracle for AlgoChicago+0x59e80 enrollment transforms. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FEATURE_BYTES 0x3c
#define PAIR_CAPACITY 42

typedef void (*correspondence_t) (void *, void *, int32_t *);
typedef void (*estimate_transform_t) (const int32_t *, const int32_t *, int32_t,
                                      int32_t *, uint8_t *, int32_t *);

typedef struct
{
  uint32_t count;
  uint8_t *records;
} FeatureSet;

static int
read_features (const char *path,
               FeatureSet *set)
{
  FILE *file = fopen (path, "rb");
  uint32_t ignored[2];
  long size;

  if (!file || fseek (file, 0, SEEK_END) != 0)
    return 0;
  size = ftell (file);
  if (size < 12 || fseek (file, 0, SEEK_SET) != 0 ||
      fread (&set->count, sizeof (set->count), 1, file) != 1 ||
      fread (ignored, sizeof (ignored), 1, file) != 1 ||
      size != 12 + (long) set->count * FEATURE_BYTES)
    {
      fclose (file);
      return 0;
    }
  set->records = malloc ((size_t) set->count * FEATURE_BYTES);
  if (set->count != 0 &&
      (!set->records ||
       fread (set->records, FEATURE_BYTES, set->count, file) != set->count))
    {
      fclose (file);
      free (set->records);
      return 0;
    }
  fclose (file);
  return 1;
}

static uint16_t
record_coordinate (const uint8_t *records,
                   int32_t        index,
                   size_t         offset)
{
  uint16_t value;

  memcpy (&value, records + (size_t) index * FEATURE_BYTES + offset,
          sizeof (value));
  return value;
}

int
main (int   argc,
      char *argv[])
{
  unsigned char old_object[0x100] = { 0, };
  unsigned char new_object[0x100] = { 0, };
  FeatureSet old_set = { 0, };
  FeatureSet new_set = { 0, };
  int32_t pairs[PAIR_CAPACITY * 2];
  int32_t old_points[PAIR_CAPACITY * 2] = { 0, };
  int32_t new_points[PAIR_CAPACITY * 2] = { 0, };
  int32_t transform[6] = { 0, };
  uint8_t inliers[PAIR_CAPACITY] = { 0, };
  int32_t error = 0;
  uint32_t matched = 0;
  HMODULE chicago;
  correspondence_t find_correspondences;
  estimate_transform_t estimate_transform;
  FILE *output;

  if (argc != 4)
    {
      fprintf (stderr, "usage: %s old.features new.features output.bin\n",
               argv[0]);
      return 2;
    }
  if (!read_features (argv[1], &old_set) ||
      !read_features (argv[2], &new_set))
    {
      fprintf (stderr, "could not read feature inputs\n");
      return 1;
    }
  *(int32_t *) (old_object + 0xf0) = (int32_t) old_set.count;
  *(void **) (old_object + 0xf8) = old_set.records;
  *(int32_t *) (new_object + 0xf0) = (int32_t) new_set.count;
  *(void **) (new_object + 0xf8) = new_set.records;

  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 1;
  find_correspondences = (correspondence_t) (void *)
    ((unsigned char *) chicago + 0x5aea0);
  estimate_transform = (estimate_transform_t) (void *)
    ((unsigned char *) chicago + 0x59e80);
  find_correspondences (old_object, new_object, pairs);

  while (matched < PAIR_CAPACITY && pairs[matched * 2] >= 0)
    {
      const int32_t old_index = pairs[matched * 2];
      const int32_t new_index = pairs[matched * 2 + 1];

      old_points[matched * 2] = record_coordinate (old_set.records, old_index, 2);
      old_points[matched * 2 + 1] =
        record_coordinate (old_set.records, old_index, 4);
      new_points[matched * 2] = record_coordinate (new_set.records, new_index, 2);
      new_points[matched * 2 + 1] =
        record_coordinate (new_set.records, new_index, 4);
      matched++;
    }
  estimate_transform (new_points, old_points, (int32_t) matched, transform,
                      inliers, &error);

  output = fopen (argv[3], "wb");
  if (!output || fwrite (&matched, sizeof (matched), 1, output) != 1 ||
      fwrite (transform, sizeof (transform), 1, output) != 1 ||
      fwrite (&error, sizeof (error), 1, output) != 1 ||
      fwrite (inliers, sizeof (inliers), 1, output) != 1)
    return 1;
  fclose (output);

  printf ("matched=%u transform=%d,%d,%d,%d,%d,%d error=%d inliers=",
          matched, transform[0], transform[1], transform[2], transform[3],
          transform[4], transform[5], error);
  for (uint32_t index = 0; index < matched; index++)
    if (inliers[index])
      printf ("%u,", index);
  printf ("\n");
  return 0;
}
