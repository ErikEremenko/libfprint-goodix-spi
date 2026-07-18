// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct behavioral oracle for the shared late-rejection predicate at
 * AlgoChicago+0x2ce50.  This is intentionally linked at run time so the
 * production DLL remains unmodified.
 *
 * The baseline vector is an entry captured from the normal Engine identify
 * path.  Single-axis sweeps make the otherwise very large threshold function
 * observable without relying on a particular gallery/probe pair to reach it.
 */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int (*late_rejection_t) (int32_t *, int32_t *, int32_t, int32_t *,
                                 int32_t *, int32_t *, int32_t *);

#define CORPUS_SAMPLES_PER_TYPE 4096u
#define CORPUS_TEMPLATE_TYPE_COUNT 6u

typedef struct
{
  int32_t probe[0x120 / 4];
  int32_t record[0x68 / 4];
  int32_t context[32];
  int32_t state[3];
  int32_t reject_count;
  int32_t flag;
} rejection_vector_t;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t count;
  uint32_t reserved;
} rejection_corpus_header_t;

typedef struct
{
  int32_t width;
  int32_t height;
  int32_t probe_quality;
  int32_t template_type;
  int32_t record[0x68 / 4];
  int32_t transform[6];
  int32_t state[3];
  int32_t rejection_count_in;
  int32_t flag_in;
  int32_t rejected;
  int32_t rejection_count_out;
  int32_t flag_out;
} rejection_corpus_vector_t;

static uint32_t random_state = 0x51c02ce5u;

static uint32_t
next_random (void)
{
  random_state ^= random_state << 13;
  random_state ^= random_state >> 17;
  random_state ^= random_state << 5;
  return random_state;
}

static int32_t
random_range (int32_t minimum, int32_t maximum)
{
  return minimum + (int32_t) (next_random () %
                               (uint32_t) (maximum - minimum + 1));
}

static void
initialize_baseline (rejection_vector_t *vector)
{
  static const int32_t record[0x68 / 4] = {
    31, 31, 0, 0, 246, 253, 254, 250, 254, 137, 100, 32, 0,
    0, 0, 256, 0, 0, 0, 256, 0, 89, 100, 89, 100, 0,
  };

  memset (vector, 0, sizeof (*vector));
  vector->probe[0] = 80;
  vector->probe[1] = 64;
  vector->probe[0xf0 / 4] = 100;
  vector->probe[0x10c / 4] = 89;
  vector->probe[0x110 / 4] = 100;
  memcpy (vector->record, record, sizeof (record));
  vector->context[0] = 256;
  vector->context[4] = 256;
  vector->state[0] = 2;
  vector->state[1] = 0;
  vector->state[2] = 4;
  vector->flag = 1;
}

static void
run_vector (late_rejection_t rejection, const char *axis, int32_t value,
            rejection_vector_t *vector)
{
  int32_t rejected = rejection (vector->probe, vector->record, 24,
                                vector->context, vector->state,
                                &vector->reject_count, &vector->flag);

  printf ("axis=%s value=%ld rejected=%ld reject-count=%ld flag=%ld\n",
          axis, (long) value, (long) rejected,
          (long) vector->reject_count, (long) vector->flag);
}

static int
write_random_corpus (late_rejection_t rejection, const char *path)
{
  static const int32_t template_types[] = { 7, 10, 23, 24, 25, 26 };
  const rejection_corpus_header_t header = {
    0x34523243u, 2,
    CORPUS_SAMPLES_PER_TYPE * CORPUS_TEMPLATE_TYPE_COUNT, 0,
  };
  FILE *file = fopen (path, "wb");

  if (!file)
    {
      fprintf (stderr, "could not create rejection corpus %s\n", path);
      return 2;
    }
  fwrite (&header, sizeof (header), 1, file);
  for (uint32_t type_index = 0;
       type_index < sizeof (template_types) / sizeof (template_types[0]);
       type_index++)
    for (uint32_t sample = 0; sample < CORPUS_SAMPLES_PER_TYPE; sample++)
      {
        rejection_vector_t baseline;
        rejection_corpus_vector_t vector = { 0, };

        initialize_baseline (&baseline);
        vector.width = 80;
        vector.height = 64;
        vector.probe_quality = random_range (0, 100);
        vector.template_type = template_types[type_index];
        memcpy (vector.record, baseline.record, sizeof (vector.record));
        vector.record[0] = random_range (0, 42);
        vector.record[1] = random_range (0, 42);
        vector.record[4] = random_range (150, 260);
        vector.record[5] = random_range (100, 260);
        vector.record[6] = random_range (100, 260);
        vector.record[7] = random_range (80, 260);
        vector.record[8] = random_range (100, 260);
        vector.record[9] = random_range (0, 150);
        vector.record[10] = random_range (0, 100);
        vector.record[11] = random_range (0, 100);
        vector.record[13] = random_range (0, 1);
        vector.record[14] = random_range (0, 1);
        vector.record[21] = random_range (0, 100);
        vector.record[22] = random_range (65, 100);
        vector.record[23] = random_range (0, 100);
        vector.record[24] = random_range (65, 100);
        vector.transform[0] = random_range (180, 320);
        vector.transform[1] = random_range (-64, 64);
        vector.transform[2] = random_range (-12000, 12000);
        vector.transform[3] = random_range (-64, 64);
        vector.transform[4] = random_range (180, 320);
        vector.transform[5] = random_range (-12000, 12000);
        memcpy (&vector.record[15], vector.transform,
                sizeof (vector.transform));
        vector.state[0] = random_range (0, 8);
        vector.state[1] = random_range (0, 8);
        vector.state[2] = random_range (0, 8);
        vector.rejection_count_in = random_range (0, 8);
        vector.flag_in = random_range (0, 2);
        vector.rejection_count_out = vector.rejection_count_in;
        vector.flag_out = vector.flag_in;
        vector.rejected = rejection (
          (int32_t[]) { 80, 64, [0x43] = vector.probe_quality },
          vector.record, vector.template_type, vector.transform, vector.state,
          &vector.rejection_count_out, &vector.flag_out);
        fwrite (&vector, sizeof (vector), 1, file);
      }
  fclose (file);
  printf ("wrote shared rejection corpus: %lu vectors across 6 modes -> %s\n",
          (unsigned long) header.count, path);
  return 0;
}

int
main (void)
{
  HMODULE module = LoadLibraryA ("AlgoChicago.dll");
  late_rejection_t rejection;
  rejection_vector_t vector;

  if (!module)
    {
      fprintf (stderr, "LoadLibrary(AlgoChicago.dll) failed: %lu\n",
               GetLastError ());
      return 1;
    }
  rejection = (late_rejection_t) ((unsigned char *) module + 0x2ce50);
  if (getenv ("CHICAGO_REJECTION_VECTOR"))
    return write_random_corpus (rejection,
                                getenv ("CHICAGO_REJECTION_VECTOR"));

  initialize_baseline (&vector);
  run_vector (rejection, "baseline", 0, &vector);

  for (int32_t value = 0; value <= 260; value += 10)
    {
      initialize_baseline (&vector);
      vector.record[5] = value;
      run_vector (rejection, "record-14", value, &vector);
    }
  for (int32_t value = 0; value <= 260; value += 10)
    {
      initialize_baseline (&vector);
      vector.record[8] = value;
      run_vector (rejection, "record-20", value, &vector);
    }
  for (int32_t value = 0; value <= 260; value += 10)
    {
      initialize_baseline (&vector);
      vector.record[5] = value;
      vector.record[8] = value;
      run_vector (rejection, "record-14-20", value, &vector);
    }
  for (int32_t value = 0; value <= 100; value += 5)
    {
      initialize_baseline (&vector);
      vector.probe[0x10c / 4] = value;
      run_vector (rejection, "probe-10c", value, &vector);
    }
  for (int32_t value = 0; value <= 30; value++)
    {
      initialize_baseline (&vector);
      vector.record[0] = value;
      vector.record[5] = 217;
      vector.record[0xb] = 25;
      run_vector (rejection, "flag-record-00", value, &vector);
    }
  initialize_baseline (&vector);
  vector.record[5] = 150;
  vector.reject_count = 5;
  run_vector (rejection, "carried-count", 5, &vector);
  for (int32_t value = 0; value <= 8; value++)
    {
      initialize_baseline (&vector);
      vector.state[0] = value;
      run_vector (rejection, "state-0", value, &vector);
    }
  for (int32_t value = 0; value <= 8; value++)
    {
      initialize_baseline (&vector);
      vector.state[1] = value;
      run_vector (rejection, "state-1", value, &vector);
    }
  for (int32_t value = 0; value <= 8; value++)
    {
      initialize_baseline (&vector);
      vector.state[2] = value;
      run_vector (rejection, "state-2", value, &vector);
    }
  return 0;
}
