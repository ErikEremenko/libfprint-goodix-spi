// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct randomized oracle for AlgoChicago+0x245c0, specialized to type 24. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int64_t (*prefilter_t) (void *, int32_t *, int32_t, void *, int32_t *);

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t count;
  uint32_t reserved;
} corpus_header_t;

typedef struct
{
  int32_t record[0x68 / 4];
  int32_t auxiliary_count;
  int32_t candidate_metric;
  int32_t rejected;
} corpus_vector_t;

static uint32_t
next_random (uint32_t *state)
{
  uint32_t value = *state;

  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  *state = value;
  return value;
}

int
main (int   argc,
      char *argv[])
{
  corpus_header_t header = { 0x46504334, 1, 512, 0 };
  corpus_vector_t vectors[512] = { 0, };
  int32_t probe[0x160 / 4] = { 0, };
  int32_t context[24] = { 0, };
  uint32_t random_state = 0x51c0245c;
  HMODULE chicago;
  prefilter_t prefilter;
  FILE *output;

  if (argc != 2)
    {
      fprintf (stderr, "usage: %s vectors.bin\n", argv[0]);
      return 2;
    }
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 3;
  prefilter = (prefilter_t) ((unsigned char *) chicago + 0x245c0);
  for (uint32_t index = 0; index < header.count; index++)
    {
      int32_t state[5] = { 0, };

      for (uint32_t field = 0; field < 0x68 / 4; field++)
        vectors[index].record[field] =
          (int32_t) (next_random (&random_state) % 301);
      vectors[index].auxiliary_count =
        (int32_t) (next_random (&random_state) % 8);
      vectors[index].candidate_metric =
        (int32_t) (next_random (&random_state) % 8);
      state[0] = vectors[index].auxiliary_count;
      state[1] = vectors[index].candidate_metric;
      vectors[index].rejected = (int32_t) prefilter (
        probe, vectors[index].record, 24, context, state);
    }
  output = fopen (argv[1], "wb");
  if (!output || fwrite (&header, sizeof (header), 1, output) != 1 ||
      fwrite (vectors, sizeof (vectors), 1, output) != 1 ||
      fclose (output) != 0)
    return 4;
  printf ("wrote %u official type-24 prefilter vectors\n", header.count);
  return 0;
}
