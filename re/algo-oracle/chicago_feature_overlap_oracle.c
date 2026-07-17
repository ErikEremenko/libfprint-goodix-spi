// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct offline oracle for AlgoChicago+0x5ab10 feature-overlap statistics. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RECORD_BYTES 0x3c
#define RECORD_LIMIT 180

typedef int (*template_unpack_t) (void *, int, void *, void **);
typedef void (*template_delete_t) (void *);
typedef void (*feature_overlap_t) (void *, void *, const int32_t *, int32_t,
                                   int32_t *, const uint8_t *, int32_t);

typedef struct
{
  uint32_t magic;
  uint32_t version;
  int32_t transform[6];
  int32_t geometry_count;
  int32_t template_type;
  uint32_t gallery_count;
  uint32_t probe_count;
  int32_t statistics[11];
  uint8_t gallery_records[RECORD_LIMIT * RECORD_BYTES];
  uint8_t probe_records[RECORD_LIMIT * RECORD_BYTES];
} feature_overlap_vector_t;

static unsigned char *
read_file (const char *path,
           size_t     *size)
{
  FILE *input = fopen (path, "rb");
  unsigned char *contents;
  long length;

  if (!input || fseek (input, 0, SEEK_END) != 0 ||
      (length = ftell (input)) <= 0 || fseek (input, 0, SEEK_SET) != 0)
    return NULL;
  contents = malloc ((size_t) length);
  if (!contents || fread (contents, 1, (size_t) length, input) !=
                   (size_t) length)
    return NULL;
  fclose (input);
  *size = (size_t) length;
  return contents;
}

static int
write_subtemplate (const char *path,
                   const uint8_t *subtemplate)
{
  const uint32_t magic = 0x4a424f53;
  FILE *output = fopen (path, "wb");

  if (!output || fwrite (&magic, sizeof (magic), 1, output) != 1 ||
      fwrite (subtemplate, 1, 0x160, output) != 0x160)
    return 0;
  for (size_t index = 0; index < 3; index++)
    {
      const uint8_t *object = *(uint8_t * const *)
        (subtemplate + 8 + index * 8);
      const uint32_t bytes = object ? *(const uint32_t *) (object + 12) : 0;
      const void *data = object ? *(void * const *) (object + 24) : NULL;

      if (!object || fwrite (&bytes, sizeof (bytes), 1, output) != 1 ||
          fwrite (object, 1, 32, output) != 32 ||
          (bytes != 0 && fwrite (data, 1, bytes, output) != bytes))
        return 0;
    }
  return fclose (output) == 0;
}

int
main (int argc,
      char **argv)
{
  HMODULE chicago;
  template_unpack_t unpack;
  template_delete_t delete_template;
  feature_overlap_t feature_overlap;
  unsigned char *gallery_packed;
  unsigned char *probe_packed;
  size_t gallery_size;
  size_t probe_size;
  void *gallery = NULL;
  void *probe = NULL;
  uint8_t *gallery_inner;
  uint8_t *probe_inner;
  uint8_t *gallery_subtemplate;
  uint8_t *probe_subtemplate;
  const char *geometry_value;
  const char *transform_values;
  feature_overlap_vector_t *vector;
  FILE *output;

  if (argc != 4)
    {
      fprintf (stderr, "usage: %s gallery.bin probe.bin output.bin\n", argv[0]);
      return 2;
    }
  vector = calloc (1, sizeof (*vector));
  gallery_packed = read_file (argv[1], &gallery_size);
  probe_packed = read_file (argv[2], &probe_size);
  if (!vector || !gallery_packed || !probe_packed)
    return 3;
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 4;
  unpack = (template_unpack_t) GetProcAddress (chicago, "templateUnPack");
  delete_template =
    (template_delete_t) GetProcAddress (chicago, "templateDelete");
  feature_overlap =
    (feature_overlap_t) ((unsigned char *) chicago + 0x5ab10);
  if (!unpack || !delete_template ||
      unpack (gallery_packed, (int) gallery_size, NULL, &gallery) != 0 ||
      unpack (probe_packed, (int) probe_size, NULL, &probe) != 0)
    return 5;
  gallery_inner = *(uint8_t **) gallery;
  probe_inner = *(uint8_t **) probe;
  gallery_subtemplate = *(uint8_t **) (gallery_inner + 0x30);
  probe_subtemplate = *(uint8_t **) (probe_inner + 0x30);
  if (getenv ("CHICAGO_GALLERY_SOBJ") &&
      !write_subtemplate (getenv ("CHICAGO_GALLERY_SOBJ"),
                          gallery_subtemplate))
    return 8;
  if (getenv ("CHICAGO_PROBE_SOBJ") &&
      !write_subtemplate (getenv ("CHICAGO_PROBE_SOBJ"), probe_subtemplate))
    return 8;
  vector->magic = 0x504f4643;
  vector->version = 1;
  vector->transform[0] = 0x100;
  vector->transform[4] = 0x100;
  transform_values = getenv ("CHICAGO_TRANSFORM_VALUES");
  if (transform_values &&
      sscanf (transform_values, "%d,%d,%d,%d,%d,%d",
              &vector->transform[0], &vector->transform[1],
              &vector->transform[2], &vector->transform[3],
              &vector->transform[4], &vector->transform[5]) != 6)
    return 6;
  vector->geometry_count = 31;
  geometry_value = getenv ("CHICAGO_GEOMETRY_COUNT");
  if (geometry_value)
    vector->geometry_count = atoi (geometry_value);
  vector->template_type = *(int32_t *) (gallery_inner + 8);
  vector->gallery_count = *(uint32_t *) (gallery_subtemplate + 0xf0);
  vector->probe_count = *(uint32_t *) (probe_subtemplate + 0xf0);
  if (vector->gallery_count > RECORD_LIMIT || vector->probe_count > RECORD_LIMIT)
    return 6;
  memcpy (vector->gallery_records,
          *(void **) (gallery_subtemplate + 0xf8),
          vector->gallery_count * RECORD_BYTES);
  memcpy (vector->probe_records,
          *(void **) (probe_subtemplate + 0xf8),
          vector->probe_count * RECORD_BYTES);
  feature_overlap (gallery_subtemplate, probe_subtemplate, vector->transform,
                   vector->geometry_count, vector->statistics, NULL,
                   vector->template_type);
  output = fopen (argv[3], "wb");
  if (!output || fwrite (vector, 1, sizeof (*vector), output) !=
                 sizeof (*vector) || fclose (output) != 0)
    return 7;
  printf ("eligible=%d matched=%d match-percent=%d geometry-percent=%d "
          "mean-distance=%d special=%d counts=%u/%u type=%d\n",
          vector->statistics[0], vector->statistics[1],
          vector->statistics[2], vector->statistics[3],
          vector->statistics[4], vector->statistics[5],
          vector->gallery_count, vector->probe_count, vector->template_type);
  printf ("gallery-dimensions=%d,%d probe-dimensions=%d,%d\n",
          *(int32_t *) gallery_subtemplate,
          *(int32_t *) (gallery_subtemplate + 4),
          *(int32_t *) probe_subtemplate,
          *(int32_t *) (probe_subtemplate + 4));
  delete_template (probe);
  delete_template (gallery);
  free (vector);
  return 0;
}
