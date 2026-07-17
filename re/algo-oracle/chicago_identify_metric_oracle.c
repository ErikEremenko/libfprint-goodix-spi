// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct offline oracle for AlgoChicago+0x510e0 identify metrics. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_BYTES 160
#define MASK_BYTES 40

typedef int (*template_unpack_t) (void *, int, void *, void **);
typedef void (*template_delete_t) (void *);
typedef int32_t (*metric_t) (void *, void *, const int32_t *, const int32_t *,
                             int32_t *, int32_t *, int32_t *, int32_t *,
                             int32_t, int32_t *, int32_t *);

typedef struct
{
  uint8_t primary[MAP_BYTES];
  uint8_t secondary[MAP_BYTES];
  uint8_t validity[MAP_BYTES];
  uint8_t coarse_mask[MASK_BYTES];
  uint8_t position_map[MAP_BYTES];
} metric_data_t;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  int32_t transform[6];
  int32_t outputs[5];
  metric_data_t gallery;
  metric_data_t probe;
} identify_metric_vector_t;

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
copy_metric_data (const uint8_t *subtemplate,
                  metric_data_t *data)
{
  for (size_t index = 0; index < 3; index++)
    {
      const uint8_t *object = *(uint8_t * const *)
        (subtemplate + 8 + index * 8);
      const uint8_t *bytes;

      if (!object || *(const uint32_t *) (object + 12) != MAP_BYTES)
        return 0;
      bytes = *(uint8_t * const *) (object + 24);
      memcpy ((uint8_t *) data + index * MAP_BYTES, bytes, MAP_BYTES);
    }
  memcpy (data->coarse_mask, subtemplate + 0x28, MASK_BYTES);
  if (*(uint8_t * const *) (subtemplate + 0x130))
    {
      const uint8_t *object = *(uint8_t * const *) (subtemplate + 0x130);

      if (*(const uint32_t *) (object + 12) == MAP_BYTES)
        memcpy (data->position_map,
                *(uint8_t * const *) (object + 24), MAP_BYTES);
    }
  return 1;
}

int
main (int argc,
      char **argv)
{
  HMODULE chicago;
  template_unpack_t unpack;
  template_delete_t delete_template;
  metric_t metric;
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
  int32_t config[3] = { 1, 0, 1 };
  identify_metric_vector_t vector = { 0, };
  FILE *output;

  if (argc != 4)
    {
      fprintf (stderr, "usage: %s gallery.bin probe.bin output.bin\n", argv[0]);
      return 2;
    }
  gallery_packed = read_file (argv[1], &gallery_size);
  probe_packed = read_file (argv[2], &probe_size);
  if (!gallery_packed || !probe_packed)
    return 3;
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 4;
  unpack = (template_unpack_t) GetProcAddress (chicago, "templateUnPack");
  delete_template =
    (template_delete_t) GetProcAddress (chicago, "templateDelete");
  metric = (metric_t) ((unsigned char *) chicago + 0x510e0);
  if (!unpack || !delete_template ||
      unpack (gallery_packed, (int) gallery_size, NULL, &gallery) != 0 ||
      unpack (probe_packed, (int) probe_size, NULL, &probe) != 0)
    return 5;
  gallery_inner = *(uint8_t **) gallery;
  probe_inner = *(uint8_t **) probe;
  gallery_subtemplate = *(uint8_t **) (gallery_inner + 0x30);
  probe_subtemplate = *(uint8_t **) (probe_inner + 0x30);
  vector.magic = 0x544d4943;
  vector.version = 1;
  vector.transform[0] = 0x100;
  vector.transform[4] = 0x100;
  if (getenv ("CHICAGO_TRANSFORM_VALUES") &&
      sscanf (getenv ("CHICAGO_TRANSFORM_VALUES"), "%d,%d,%d,%d,%d,%d",
              &vector.transform[0], &vector.transform[1],
              &vector.transform[2], &vector.transform[3],
              &vector.transform[4], &vector.transform[5]) != 6)
    return 6;
  if (!copy_metric_data (gallery_subtemplate, &vector.gallery) ||
      !copy_metric_data (probe_subtemplate, &vector.probe))
    return 7;
  vector.outputs[0] = metric (
    probe_subtemplate, gallery_subtemplate, vector.transform, config,
    &vector.outputs[1], &vector.outputs[2], &vector.outputs[3],
    &vector.outputs[4], 0, NULL, NULL);
  output = fopen (argv[3], "wb");
  if (!output || fwrite (&vector, 1, sizeof (vector), output) !=
                 sizeof (vector) || fclose (output) != 0)
    return 8;
  printf ("selector=%d outputs=%d,%d,%d,%d\n",
          vector.outputs[0], vector.outputs[1], vector.outputs[2],
          vector.outputs[3], vector.outputs[4]);
  delete_template (probe);
  delete_template (gallery);
  return 0;
}
