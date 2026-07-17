// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct oracle for AlgoChicago+0x5bf50 identify candidate distances. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORD_BYTES 60
#define MATRIX_STRIDE 180

typedef void (*candidate_t) (void *, void *, int32_t *, int32_t *, int32_t *,
                             uint8_t *, uint8_t *);
typedef void (*selector_t) (void *, int32_t *, int32_t *, int, int, int32_t *,
                            int32_t *);

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
} vector_header_t;

static unsigned char *
read_features (const char *path,
               uint32_t   *count)
{
  FILE *input = fopen (path, "rb");
  unsigned char *contents;
  long length;

  if (!input || fseek (input, 0, SEEK_END) != 0 ||
      (length = ftell (input)) < 12 || fseek (input, 0, SEEK_SET) != 0)
    return NULL;
  contents = malloc ((size_t) length);
  if (!contents || fread (contents, 1, (size_t) length, input) !=
                   (size_t) length)
    return NULL;
  fclose (input);
  memcpy (count, contents, sizeof (*count));
  if ((size_t) length != 12 + (size_t) *count * RECORD_BYTES)
    return NULL;
  memmove (contents, contents + 12, (size_t) *count * RECORD_BYTES);
  return contents;
}

int
main (int argc,
      char **argv)
{
  HMODULE chicago;
  candidate_t candidate;
  selector_t selector;
  unsigned char *old_records;
  unsigned char *new_records;
  int32_t *best;
  int32_t *indices;
  uint8_t *distances;
  uint8_t *directions;
  int32_t *pairs;
  int32_t selector_config[4] = { 0, 0, 40, 38 };
  vector_header_t header = { 0, };
  FILE *output;
  size_t matrix_bytes;

  if (argc != 4)
    {
      fprintf (stderr, "usage: %s old.features new.features out.bin\n",
               argv[0]);
      return 2;
    }
  old_records = read_features (argv[1], &header.old_count);
  new_records = read_features (argv[2], &header.new_count);
  if (!old_records || !new_records || header.new_count > MATRIX_STRIDE)
    return 3;
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 4;
  candidate = (candidate_t) ((unsigned char *) chicago + 0x5bf50);
  selector = (selector_t) ((unsigned char *) chicago + 0x5a7b0);
  header.magic = 0x43444943; /* CIDC */
  header.version = 2;
  header.config[2] = 0;
  header.config[3] = (int32_t) header.old_count;
  header.config[4] = 0;
  header.config[5] = (int32_t) header.new_count;
  header.config[6] = getenv ("CHICAGO_MATCH_FIRST_GATE") ?
    atoi (getenv ("CHICAGO_MATCH_FIRST_GATE")) : 64;
  header.config[7] = getenv ("CHICAGO_MATCH_COMBINED_GATE") ?
    atoi (getenv ("CHICAGO_MATCH_COMBINED_GATE")) : 100;
  header.selector_limit = 31;
  header.best_multiplier = selector_config[2];
  header.second_multiplier = selector_config[3];
  best = malloc ((size_t) header.old_count * 2 * sizeof (*best));
  indices = malloc ((size_t) header.old_count * 2 * sizeof (*indices));
  matrix_bytes = (size_t) header.old_count * MATRIX_STRIDE;
  distances = malloc (matrix_bytes);
  directions = calloc (1, matrix_bytes);
  pairs = malloc (header.selector_limit * 2 * sizeof (*pairs));
  if (!best || !indices || !distances || !directions || !pairs)
    return 5;
  for (uint32_t index = 0; index < header.old_count * 2; index++)
    {
      best[index] = 192;
      indices[index] = -1;
    }
  memset (distances, 0xff, matrix_bytes);
  candidate (old_records, new_records, header.config, best, indices,
             distances, directions);
  selector (new_records, best, indices, (int) header.old_count,
            (int) header.selector_limit, selector_config, pairs);
  while (header.selected_count < header.selector_limit &&
         pairs[header.selected_count * 2] >= 0)
    header.selected_count++;
  output = fopen (argv[3], "wb");
  if (!output)
    return 6;
  fwrite (&header, sizeof (header), 1, output);
  fwrite (best, sizeof (*best), header.old_count * 2, output);
  fwrite (indices, sizeof (*indices), header.old_count * 2, output);
  fwrite (distances, 1, matrix_bytes, output);
  fwrite (directions, 1, matrix_bytes, output);
  fwrite (pairs, sizeof (*pairs), header.selector_limit * 2, output);
  fclose (output);
  printf ("old=%u new=%u gates=%d/%d", header.old_count, header.new_count,
          header.config[6], header.config[7]);
  for (uint32_t index = 0; index < header.old_count && index < 8; index++)
    printf (" [%u]=%d:%d,%d:%d", index, best[index * 2], indices[index * 2],
            best[index * 2 + 1], indices[index * 2 + 1]);
  printf (" selected=%u\n", header.selected_count);
  return 0;
}
