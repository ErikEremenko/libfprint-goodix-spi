// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct offline oracle for AlgoChicago+0x5aea0 descriptor correspondence. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FEATURE_BYTES 0x3c
#define PAIR_CAPACITY 42

typedef void (*correspondence_t) (void *, void *, int32_t *);

typedef struct
{
  uint32_t count;
  uint32_t quality;
  uint32_t coverage;
  uint8_t *records;
} FeatureSet;

static int
read_features (const char *path,
               FeatureSet *set)
{
  FILE *file = fopen (path, "rb");
  long size;

  if (!file || fseek (file, 0, SEEK_END) != 0)
    return 0;
  size = ftell (file);
  if (size < 12 || fseek (file, 0, SEEK_SET) != 0 ||
      fread (&set->count, sizeof (set->count), 1, file) != 1 ||
      fread (&set->quality, sizeof (set->quality), 1, file) != 1 ||
      fread (&set->coverage, sizeof (set->coverage), 1, file) != 1 ||
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

int
main (int   argc,
      char *argv[])
{
  unsigned char old_object[0x100] = { 0, };
  unsigned char new_object[0x100] = { 0, };
  FeatureSet old_set = { 0, };
  FeatureSet new_set = { 0, };
  int32_t pairs[PAIR_CAPACITY * 2];
  uint32_t matched = 0;
  HMODULE chicago;
  correspondence_t find_correspondences;
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
    {
      fprintf (stderr, "could not load AlgoChicago.dll\n");
      return 1;
    }
  find_correspondences = (correspondence_t) (void *)
    ((unsigned char *) chicago + 0x5aea0);
  find_correspondences (old_object, new_object, pairs);
  while (matched < PAIR_CAPACITY && pairs[matched * 2] >= 0)
    matched++;

  output = fopen (argv[3], "wb");
  if (!output ||
      fwrite (&old_set.count, sizeof (old_set.count), 1, output) != 1 ||
      fwrite (&new_set.count, sizeof (new_set.count), 1, output) != 1 ||
      fwrite (&matched, sizeof (matched), 1, output) != 1 ||
      fwrite (pairs, sizeof (pairs), 1, output) != 1)
    {
      fprintf (stderr, "could not write %s\n", argv[3]);
      return 1;
    }
  fclose (output);

  printf ("old=%u new=%u matched=%u", old_set.count, new_set.count, matched);
  for (uint32_t index = 0; index < matched; index++)
    printf (" %d:%d", pairs[index * 2], pairs[index * 2 + 1]);
  printf ("\n");
  return 0;
}
