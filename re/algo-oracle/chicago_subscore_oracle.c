// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct oracle for AlgoChicago+0x29240 per-subtemplate score inputs. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*template_unpack_t) (void *, int, void *, void **);
typedef void (*template_delete_t) (void *);
typedef int (*score_config_t) (void *, void *, int, int);
typedef void (*subscore_t) (void *, void *, uint8_t *, uint8_t *, void *,
                            void *, int32_t *, int, int32_t *, void *);

typedef struct
{
  uint32_t magic;
  uint32_t version;
  int32_t transform[6];
  uint8_t result[0x150];
  uint8_t work[0x3070];
} subscore_vector_t;

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

int
main (int argc,
      char **argv)
{
  HMODULE chicago;
  template_unpack_t unpack;
  template_delete_t delete_template;
  subscore_t subscore;
  score_config_t build_score_config;
  unsigned char *gallery_packed;
  unsigned char *probe_packed;
  uint8_t *distances;
  uint8_t *directions;
  subscore_vector_t vector = { 0, };
  int32_t config[3];
  int config_index = 0;
  int gallery_index = 0;
  uint8_t score_config[0x58] = { 0, };
  void *gallery = NULL;
  void *probe = NULL;
  void *gallery_inner;
  void *probe_inner;
  void *probe_subtemplate;
  size_t gallery_size;
  size_t probe_size;
  FILE *output;

  if (argc != 4)
    {
      fprintf (stderr, "usage: %s gallery.bin probe.bin output.bin\n", argv[0]);
      return 2;
    }
  gallery_packed = read_file (argv[1], &gallery_size);
  probe_packed = read_file (argv[2], &probe_size);
  distances = malloc (0x7e90);
  directions = calloc (1, 0x7e90);
  if (!gallery_packed || !probe_packed || !distances || !directions)
    return 3;
  memset (distances, 0xff, 0x7e90);
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 4;
  unpack = (template_unpack_t) GetProcAddress (chicago, "templateUnPack");
  delete_template =
    (template_delete_t) GetProcAddress (chicago, "templateDelete");
  subscore = (subscore_t) ((unsigned char *) chicago + 0x29240);
  build_score_config =
    (score_config_t) ((unsigned char *) chicago + 0x2fcb0);
  if (!unpack || !delete_template ||
      unpack (gallery_packed, (int) gallery_size, NULL, &gallery) != 0 ||
      unpack (probe_packed, (int) probe_size, NULL, &probe) != 0)
    return 5;
  gallery_inner = *(void **) gallery;
  probe_inner = *(void **) probe;
  probe_subtemplate = *(void **) ((unsigned char *) probe_inner + 0x30);
  config[0] = *(int32_t *) ((unsigned char *) gallery_inner + 0x14);
  config[1] = 0;
  config[2] = 1;
  vector.magic = 0x42555343;
  vector.version = 1;
  vector.transform[0] = 0x100;
  vector.transform[4] = 0x100;
  if (getenv ("CHICAGO_CONFIG_INDEX"))
    config_index = atoi (getenv ("CHICAGO_CONFIG_INDEX"));
  if (getenv ("CHICAGO_GALLERY_INDEX"))
    gallery_index = atoi (getenv ("CHICAGO_GALLERY_INDEX"));
  if (build_score_config (gallery_inner, score_config, config_index, 0) != 0)
    return 7;
  subscore (gallery_inner, probe_subtemplate, distances, directions,
            vector.work, score_config, config, gallery_index, vector.transform,
            vector.result);
  memcpy (vector.work, score_config, sizeof (score_config));
  output = fopen (argv[3], "wb");
  if (!output || fwrite (&vector, 1, sizeof (vector), output) !=
                 sizeof (vector) || fclose (output) != 0)
    return 6;
  printf ("geometry=%d/%d metric=%d/%d quality=%d/%d flags=%d,%d,%d "
          "transform=%d,%d,%d,%d,%d,%d\n",
          *(int32_t *) (vector.result + 0x00),
          *(int32_t *) (vector.result + 0x04),
          *(int32_t *) (vector.result + 0x10),
          *(int32_t *) (vector.result + 0x14),
          *(int32_t *) (vector.result + 0x28),
          *(int32_t *) (vector.result + 0x2c),
          *(int32_t *) (vector.result + 0x30),
          *(int32_t *) (vector.result + 0x34),
          *(int32_t *) (vector.result + 0x38),
          vector.transform[0], vector.transform[1], vector.transform[2],
          vector.transform[3], vector.transform[4], vector.transform[5]);
  printf ("metric-config=%d,%d,%d template-type=%d\n",
          config[0], config[1], config[2],
          *(int32_t *) ((unsigned char *) gallery_inner + 8));
  delete_template (probe);
  delete_template (gallery);
  return 0;
}
