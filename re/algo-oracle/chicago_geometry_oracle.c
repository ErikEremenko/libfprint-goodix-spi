// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct oracle for AlgoChicago+0x1fca0 identify geometric consensus. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATRIX_STRIDE 180

typedef int (*template_unpack_t) (void *, int, void *, void **);
typedef void (*template_delete_t) (void *);
typedef int (*geometry_t) (void *, void *, int, int, int, int, int32_t *,
                           int32_t *);
typedef void (*geometry_core_t) (const int32_t *, const int32_t *,
                                 const int32_t *, const int32_t *, int,
                                 int32_t *, uint8_t *, int32_t *, int, int);
typedef void (*orientation_filter_t) (void *, void *, const int32_t *,
                                      int32_t *, int32_t *, int);

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t old_count;
  uint32_t new_count;
  int32_t config[8];
  uint32_t selector_limit;
  uint32_t best_multiplier;
  uint32_t second_multiplier;
  uint32_t selected_count;
} candidate_header_t;

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
  int32_t transform[6];
  int32_t target_orientation[42];
  int32_t source_orientation[42];
  uint8_t initial[42];
  uint8_t filtered[42];
} orientation_vector_t;

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
  geometry_t geometry;
  geometry_core_t geometry_core;
  orientation_filter_t orientation_filter;
  unsigned char *gallery_packed;
  unsigned char *probe_packed;
  unsigned char *candidate_vector;
  candidate_header_t *header;
  int32_t *pairs;
  void *gallery = NULL;
  void *probe = NULL;
  void *gallery_inner;
  void *probe_inner;
  void *gallery_subtemplate;
  void *probe_subtemplate;
  int32_t detail[9] = { 0, };
  int32_t target_indices[42];
  int32_t source_indices[42];
  geometry_vector_t vector = { 0, };
  orientation_vector_t orientation_vector = { 0, };
  size_t gallery_size;
  size_t probe_size;
  size_t candidate_size;
  size_t pair_offset;
  int result;

  if (argc < 4 || argc > 6)
    {
      fprintf (stderr, "usage: %s gallery.bin probe.bin candidates.bin "
                       "[geometry-vector.bin] [orientation-vector.bin]\n",
               argv[0]);
      return 2;
    }
  gallery_packed = read_file (argv[1], &gallery_size);
  probe_packed = read_file (argv[2], &probe_size);
  candidate_vector = read_file (argv[3], &candidate_size);
  if (!gallery_packed || !probe_packed || !candidate_vector ||
      candidate_size < sizeof (*header))
    return 3;
  header = (candidate_header_t *) candidate_vector;
  if (header->magic != 0x43444943 || header->version != 2)
    return 4;
  pair_offset = sizeof (*header) + (size_t) header->old_count * 4 *
                sizeof (int32_t) + (size_t) header->old_count *
                MATRIX_STRIDE * 2;
  if (candidate_size != pair_offset + header->selector_limit * 2 *
                                     sizeof (int32_t))
    return 5;
  pairs = (int32_t *) (candidate_vector + pair_offset);
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 6;
  unpack = (template_unpack_t) GetProcAddress (chicago, "templateUnPack");
  delete_template =
    (template_delete_t) GetProcAddress (chicago, "templateDelete");
  geometry = (geometry_t) ((unsigned char *) chicago + 0x1fca0);
  geometry_core =
    (geometry_core_t) ((unsigned char *) chicago + 0x58b20);
  orientation_filter =
    (orientation_filter_t) ((unsigned char *) chicago + 0x24ce0);
  if (!unpack || !delete_template)
    return 7;
  if (unpack (gallery_packed, (int) gallery_size, NULL, &gallery) != 0 ||
      unpack (probe_packed, (int) probe_size, NULL, &probe) != 0)
    return 8;
  gallery_inner = *(void **) gallery;
  probe_inner = *(void **) probe;
  gallery_subtemplate = *(void **) ((unsigned char *) gallery_inner + 0x30);
  probe_subtemplate = *(void **) ((unsigned char *) probe_inner + 0x30);
  vector.magic = 0x4f454743;
  vector.version = 1;
  vector.mode = 2;
  vector.strict = 1;
  for (uint32_t index = 0;
       index < header->selector_limit && vector.count < 42;
       index++)
    {
      const int32_t gallery_index = pairs[index * 2];
      const int32_t probe_index = pairs[index * 2 + 1];
      const unsigned char *gallery_record;
      const unsigned char *probe_record;
      int16_t coordinate;
      int16_t orientation;
      uint32_t compact;

      if (gallery_index < 0)
        continue;
      gallery_record =
        *(const unsigned char **) ((unsigned char *) gallery_subtemplate +
                                   0xf8) + gallery_index * 0x3c;
      probe_record =
        *(const unsigned char **) ((unsigned char *) probe_subtemplate +
                                   0xf8) + probe_index * 0x3c;
      compact = vector.count++;
      target_indices[compact] = gallery_index;
      source_indices[compact] = probe_index;
      memcpy (&coordinate, probe_record + 2, sizeof (coordinate));
      vector.source_xy[compact * 2] = (uint16_t) coordinate;
      memcpy (&coordinate, probe_record + 4, sizeof (coordinate));
      vector.source_xy[compact * 2 + 1] = (uint16_t) coordinate;
      memcpy (&coordinate, gallery_record + 2, sizeof (coordinate));
      vector.target_xy[compact * 2] = (uint16_t) coordinate;
      memcpy (&coordinate, gallery_record + 4, sizeof (coordinate));
      vector.target_xy[compact * 2 + 1] = (uint16_t) coordinate;
      memcpy (&orientation, probe_record + 6, sizeof (orientation));
      vector.source_orientation[compact] = orientation;
      memcpy (&orientation, gallery_record + 6, sizeof (orientation));
      vector.target_orientation[compact] = orientation;
    }
  geometry_core (vector.source_xy, vector.target_xy,
                 vector.target_orientation, vector.source_orientation,
                 (int) vector.count, vector.transform, vector.inliers,
                 &vector.error, (int) vector.mode, (int) vector.strict);
  orientation_vector.magic = 0x524f4643;
  orientation_vector.version = 1;
  orientation_vector.count = vector.count;
  memcpy (orientation_vector.transform, vector.transform,
          sizeof (orientation_vector.transform));
  memcpy (orientation_vector.target_orientation, vector.target_orientation,
          sizeof (orientation_vector.target_orientation));
  memcpy (orientation_vector.source_orientation, vector.source_orientation,
          sizeof (orientation_vector.source_orientation));
  {
    const char *override = getenv ("CHICAGO_FILTER_TRANSFORM");
    const int force_all = getenv ("CHICAGO_FILTER_ALL") != NULL;

    if (override && sscanf (override, "%d,%d,%d,%d,%d,%d",
                            &orientation_vector.transform[0],
                            &orientation_vector.transform[1],
                            &orientation_vector.transform[2],
                            &orientation_vector.transform[3],
                            &orientation_vector.transform[4],
                            &orientation_vector.transform[5]) != 6)
      return 10;
    for (uint32_t index = 0; index < vector.count; index++)
      {
        orientation_vector.initial[index] = force_all || vector.inliers[index];
        if (!orientation_vector.initial[index])
          target_indices[index] = source_indices[index] = -1;
      }
  }
  orientation_filter (gallery_subtemplate, probe_subtemplate,
                      orientation_vector.transform, target_indices,
                      source_indices, (int) vector.count);
  for (uint32_t index = 0; index < vector.count; index++)
    orientation_vector.filtered[index] =
      target_indices[index] >= 0 && source_indices[index] >= 0;
  result = geometry (gallery_inner, probe_subtemplate, 0,
                     (int) header->selector_limit, 2, 2, pairs, detail);
  printf ("geometry=%d selected=%u detail=%d,%d,%d,%d,%d,%d\n",
          result, header->selected_count,
          detail[0], detail[1], detail[2], detail[3], detail[4], detail[5]);
  printf ("core=%u error=%d transform=%d,%d,%d,%d,%d,%d inliers=",
          vector.count, vector.error,
          vector.transform[0], vector.transform[1], vector.transform[2],
          vector.transform[3], vector.transform[4], vector.transform[5]);
  for (uint32_t index = 0; index < vector.count; index++)
    putchar (vector.inliers[index] ? '1' : '0');
  putchar ('\n');
  if (argc >= 5)
    {
      FILE *output = fopen (argv[4], "wb");

      if (!output || fwrite (&vector, 1, sizeof (vector), output) !=
                     sizeof (vector) || fclose (output) != 0)
        return 9;
    }
  if (argc == 6)
    {
      FILE *output = fopen (argv[5], "wb");

      if (!output || fwrite (&orientation_vector, 1,
                             sizeof (orientation_vector), output) !=
                     sizeof (orientation_vector) || fclose (output) != 0)
        return 11;
    }
  delete_template (probe);
  delete_template (gallery);
  return 0;
}
