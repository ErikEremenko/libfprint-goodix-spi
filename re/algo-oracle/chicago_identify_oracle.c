// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Stable direct oracle for AlgoChicago+0x290f0 identify scoring.
 *
 * This deliberately uses templateUnPack and identifytemplate's internal
 * scorer instead of identifyImageWrapper, whose EngineAdapter-owned current
 * context is not initialized by the standalone harness.
 *
 * Build:
 *   x86_64-w64-mingw32-gcc -O2 chicago_identify_oracle.c \
 *     -o chicago_identify_oracle.exe
 * Run from the directory containing the production AlgoChicago.dll:
 *   wine chicago_identify_oracle.exe gallery.bin probe.bin [vectors.bin]
 */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*template_unpack_t) (void *, int, void *, void **);
typedef void (*template_delete_t) (void *);
typedef int (*identify_template_t) (void *, void *, void *, int *);
typedef int (*identify_score_t) (int *, void *, void *, int, int, void *,
                                 void *);
typedef int (*study_mutate_t) (void *, void *, void *, int *, int);
typedef int (*template_packed_size_t) (void *);
typedef int (*template_pack_t) (void *, void *);

typedef struct
{
  int32_t return_code;
  int32_t score;
  /* +0x29820 writes a six-word transform at detail+0x08 and a final word at
   * detail+0x20.  Keep the complete nine-word result separate from scratch;
   * detail[6] used to alias the first three study-state words. */
  int32_t detail[9];
  uint8_t scratch[0x694];
} score_vector_t;

static unsigned char *score_tap;
static unsigned char score_tap_original;
static int score_tap_rearm;
static int score_tap_calls;
static unsigned long score_tap_offset;
static unsigned char *score_tap_next;
static unsigned long score_tap_next_offset;
static unsigned char *aux_return_tap;
static unsigned char aux_return_tap_original;
static int aux_return_tap_rearm;
static FILE *aux_count_vectors;
static int32_t aux_latest_transform[6];
static uint8_t aux_latest_values[6];
static int aux_latest_active;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint8_t auxiliary[6];
  uint8_t reserved[2];
  int32_t transform[6];
  int32_t counts[3];
} auxiliary_count_vector_t;

static int readable_span (const void *address, size_t length);

static LONG CALLBACK
score_tap_handler (EXCEPTION_POINTERS *exception)
{
  CONTEXT *registers = exception->ContextRecord;
  DWORD protection;

  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == aux_return_tap)
    {
      const int32_t *special =
        (const int32_t *) (uintptr_t) registers->R14;

      if (aux_count_vectors && aux_latest_active && special)
        {
          auxiliary_count_vector_t vector = { 0, };

          vector.magic = 0x43554143;
          vector.version = 1;
          memcpy (vector.auxiliary, aux_latest_values,
                  sizeof (vector.auxiliary));
          memcpy (vector.transform, aux_latest_transform,
                  sizeof (vector.transform));
          vector.counts[0] = special[1];
          vector.counts[1] = special[2];
          vector.counts[2] = special[3];
          fwrite (&vector, sizeof (vector), 1, aux_count_vectors);
          fflush (aux_count_vectors);
          printf ("aux-count-vector transform=%d,%d,%d,%d,%d,%d counts=%d,%d,%d\n",
                  vector.transform[0], vector.transform[1],
                  vector.transform[2], vector.transform[3],
                  vector.transform[4], vector.transform[5],
                  vector.counts[0], vector.counts[1], vector.counts[2]);
        }
      aux_latest_active = 0;
      VirtualProtect (aux_return_tap, 1, PAGE_EXECUTE_READWRITE, &protection);
      *aux_return_tap = aux_return_tap_original;
      FlushInstructionCache (GetCurrentProcess (), aux_return_tap, 1);
      registers->Rip = (DWORD64) (uintptr_t) aux_return_tap;
      registers->EFlags |= 0x100;
      aux_return_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }

  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == score_tap)
    {
      if (score_tap_offset == 0x28960)
        {
          const int32_t *record =
            (const int32_t *) (uintptr_t) registers->Rcx;
          const int32_t *config =
            (const int32_t *) (uintptr_t) registers->Rdx;

          printf ("score-record[%d]", score_tap_calls++);
          for (int index = 0; index < 26; index++)
            printf (" %02x=%d", index * 4, record[index]);
          printf (" config-type=%d config-flag=%d\n",
                  config[15], config[17]);
        }
      else if (score_tap_offset == 0x510e0)
        {
          const int32_t *transform =
            (const int32_t *) (uintptr_t) registers->R8;
          const int32_t *config =
            (const int32_t *) (uintptr_t) registers->R9;
          const int32_t *special = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x58);

          aux_latest_active = special && special[0] == 1;
          if (aux_latest_active)
            {
              const uint8_t *map = *(const uint8_t *const *)
                ((const uint8_t *) special + 0x18);

              memcpy (aux_latest_transform, transform,
                      sizeof (aux_latest_transform));
              if (map)
                memcpy (aux_latest_values, map,
                        sizeof (aux_latest_values));
              else
                memset (aux_latest_values, 0,
                        sizeof (aux_latest_values));
            }

          printf ("metric-entry[%d] config=%d,%d,%d transform=%d,%d,%d,%d,%d,%d special=%d\n",
                  score_tap_calls++, config[0], config[1], config[2],
                  transform[0], transform[1], transform[2], transform[3],
                  transform[4], transform[5], special ? special[0] : -1);
        }
      else if (score_tap_offset == 0x245c0)
        {
          const int32_t *record =
            (const int32_t *) (uintptr_t) registers->Rdx;
          const int32_t *control = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x28);

          printf ("policy-entry[%d] type=%d control=%d,%d record",
                  score_tap_calls++, (int32_t) registers->R8,
                  control[0], control[1]);
          for (int index = 0; index < 26; index++)
            printf (" %02x=%d", index * 4, record[index]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x24cdd)
        {
          printf ("policy-return[%d]=%d\n", score_tap_calls++,
                  (int32_t) registers->Rax);
        }
      else if (score_tap_offset == 0x2a5da)
        {
          const int32_t *record =
            (const int32_t *) (uintptr_t) registers->Rbx;
          const int64_t *frame =
            (const int64_t *) (uintptr_t) registers->Rbp;

          printf ("accepted[%d] gallery=%lld record",
                  score_tap_calls++, (long long) frame[-0x58 / 8]);
          for (int index = 0; index < 26; index++)
            printf (" %02x=%d", index * 4, record[index]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x251d0)
        {
          const int32_t *record =
            (const int32_t *) (uintptr_t) registers->Rcx;
          const unsigned char *probe =
            (const unsigned char *) (uintptr_t) registers->Rdx;
          const unsigned char *derived_map = NULL;
          const int32_t *stack =
            (const int32_t *) (uintptr_t) registers->Rsp;

          if (readable_span (probe, 0x164))
            memcpy (&derived_map, probe + 0x148, sizeof (derived_map));

          printf ("scheduler-helper-entry[%d] r8=%d r9=%d arg5=%d type=%d "
                  "out6=%d out7=%d probe-c7=%d v158=%d v15c=%d v160=%d "
                  "map148=%p prefix=",
                  score_tap_calls++, (int32_t) registers->R8,
                  (int32_t) registers->R9, stack[0x28 / 4],
                  stack[0x40 / 4],
                  **(const int32_t **) (uintptr_t) (registers->Rsp + 0x30),
                  **(const int32_t **) (uintptr_t) (registers->Rsp + 0x38),
                  *(const int32_t *) (probe + 0x140),
                  *(const int32_t *) (probe + 0x158),
                  *(const int32_t *) (probe + 0x15c),
                  *(const int32_t *) (probe + 0x160), derived_map);
          if (readable_span (derived_map, 16))
            for (int byte = 0; byte < 16; byte++)
              printf ("%s%u", byte ? "," : "", derived_map[byte]);
          else
            printf ("unreadable");
          printf (" record");
          for (int index = 0; index < 26; index++)
            printf (" %02x=%d", index * 4, record[index]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x2a3e9)
        {
          const int32_t *frame =
            (const int32_t *) (uintptr_t) registers->Rbp;

          printf ("study-counts[%d] zero=%d mixed=%d one=%d\n",
                  score_tap_calls++, frame[0x104 / 4], frame[0x108 / 4],
                  frame[0x10c / 4]);
        }
      else if (score_tap_offset == 0x1cd80)
        {
          const int32_t *probe =
            (const int32_t *) (uintptr_t) registers->Rcx;
          const int32_t *record =
            (const int32_t *) (uintptr_t) registers->Rdx;
          const int32_t *context =
            (const int32_t *) (uintptr_t) registers->R9;
          const int32_t *state = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x28);
          const int32_t *reject_count = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x30);
          const int32_t *flag = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x38);

          printf ("rejection-filter-entry[%d] type=%d state=%d,%d,%d "
                  "reject-count=%d flag=%d probe-qc=%d,%d probe-f0=%d "
                  "context=",
                  score_tap_calls++, (int32_t) registers->R8,
                  state[0], state[1], state[2],
                  reject_count ? *reject_count : -999,
                  flag ? *flag : -999,
                  probe[0x10c / 4], probe[0x110 / 4], probe[0xf0 / 4]);
          for (int index = 0; index < 24; index++)
            printf ("%s%d", index ? "," : "", context[index]);
          printf (" record=");
          for (int index = 0; index < 26; index++)
            printf ("%s%d", index ? "," : "", record[index]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x1d4fa)
        {
          const int32_t *reject_count = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x48);
          const int32_t *flag = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x38);

          printf ("rejection-filter-return[%d] rejected=%d "
                  "reject-count=%d flag=%d\n",
                  score_tap_calls++, (int32_t) registers->Rbp,
                  reject_count ? *reject_count : -999,
                  flag ? *flag : -999);
        }
      else if (score_tap_offset == 0x2ce50)
        {
          const int32_t *probe =
            (const int32_t *) (uintptr_t) registers->Rcx;
          const int32_t *record =
            (const int32_t *) (uintptr_t) registers->Rdx;
          const int32_t *context =
            (const int32_t *) (uintptr_t) registers->R9;
          const int32_t *state = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x28);
          const int32_t *reject_count = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x30);
          const int32_t *flag = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x38);

          printf ("type24-rejection-entry[%d] type=%d state=%d,%d,%d "
                  "reject-count=%d flag=%d probe-qc=%d,%d probe-f0=%d "
                  "probe-wh=%d,%d context=",
                  score_tap_calls++, (int32_t) registers->R8,
                  state[0], state[1], state[2],
                  reject_count ? *reject_count : -999,
                  flag ? *flag : -999,
                  probe[0x10c / 4], probe[0x110 / 4], probe[0xf0 / 4],
                  probe[0], probe[1]);
          for (int index = 0; index < 24; index++)
            printf ("%s%d", index ? "," : "", context[index]);
          printf (" record=");
          for (int index = 0; index < 26; index++)
            printf ("%s%d", index ? "," : "", record[index]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x2fa6d)
        {
          const int32_t *reject_count = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x48);
          const int32_t *flag = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x50);

          printf ("type24-rejection-return[%d] rejected=%d "
                  "reject-count=%d flag=%d\n",
                  score_tap_calls++, (int32_t) registers->R9,
                  reject_count ? *reject_count : -999,
                  flag ? *flag : -999);
        }
      else if (score_tap_offset == 0x26270)
        {
          const int32_t *record =
            (const int32_t *) (uintptr_t) registers->Rcx;
          const int32_t *context =
            (const int32_t *) (uintptr_t) registers->R9;
          const int32_t *stack =
            (const int32_t *) (uintptr_t) registers->Rsp;

          printf ("evidence-entry[%d] quality=%d,%d arg5=%d "
                  "context=%d,%d,%d,%d record",
                  score_tap_calls++, (int32_t) registers->Rdx,
                  (int32_t) registers->R8, stack[0x28 / 4],
                  context[0], context[1], context[0x3c / 4],
                  context[0x40 / 4]);
          for (int index = 0; index < 26; index++)
            printf (" %02x=%d", index * 4, record[index]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x265a8)
        {
          const int32_t *out8 = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0xd8);

          printf ("evidence-return[%d] out6=%d out7=%d out8=%d\n",
                  score_tap_calls++,
                  **(const int32_t **) (uintptr_t) (registers->Rsp + 0xc8),
                  **(const int32_t **) (uintptr_t) (registers->Rsp + 0xd0),
                  out8 ? *out8 : -999);
        }
      else if (score_tap_offset == 0x27a65)
        {
          printf ("fallback-geometry[%d] gallery=%d geometry=%d\n",
                  score_tap_calls++, (int32_t) registers->Rsi,
                  (int32_t) registers->Rax);
        }
      else if (score_tap_offset == 0x27a60)
        {
          const int32_t *pairs = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x30);

          printf ("fallback-pairs[%d] gallery=%d",
                  score_tap_calls++, (int32_t) registers->Rsi);
          for (int index = 0; index < 31; index++)
            printf (" %d:%d", pairs[index * 2], pairs[index * 2 + 1]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x27aed)
        {
          const int32_t *frame =
            (const int32_t *) (uintptr_t) registers->Rbp;

          printf ("fallback-metric[%d] gallery=%d geometry=%d selector=%d "
                  "agreement=%d coverage=%d\n",
                  score_tap_calls++, (int32_t) registers->Rsi,
                  (int32_t) registers->Rdi, (int32_t) registers->Rax,
                  frame[-0x3c / 4], frame[-0x40 / 4]);
        }
      else if (score_tap_offset == 0x27b98)
        {
          const int32_t *frame =
            (const int32_t *) (uintptr_t) registers->Rbp;

          printf ("fallback-accepted[%d] gallery=%d geometry=%d agreement=%d "
                  "coverage=%d best-agreement=%d best-coverage=%d\n",
                  score_tap_calls++, (int32_t) registers->Rsi,
                  (int32_t) registers->Rdi, frame[-0x3c / 4],
                  frame[-0x40 / 4], frame[0x14 / 4], frame[0x24 / 4]);
        }
      else if (score_tap_offset == 0x2a891)
        {
          const int32_t *scratch =
            (const int32_t *) (uintptr_t) registers->Rsi;
          const int32_t *gallery_state =
            (const int32_t *) (uintptr_t) registers->R12;

          printf ("gallery-mask-check[%d] gallery=%d scratch684=%d "
                  "state34=%d count=%d\n",
                  score_tap_calls++, (int32_t) registers->R10,
                  scratch[0x684 / 4], gallery_state[0x34 / 4],
                  (int32_t) registers->Rcx);
        }
      else if (score_tap_offset == 0x278f0)
        {
          const int32_t *mask = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x30);
          const int32_t *gallery =
            (const int32_t *) (uintptr_t) registers->R9;
          const int32_t count = gallery[0x24 / 4];

          printf ("gallery-mask[%d] count=%d", score_tap_calls++, count);
          for (int index = 0; index < count; index++)
            printf (" %d", mask[index]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x5bf50)
        {
          const int32_t *config =
            (const int32_t *) (uintptr_t) registers->R8;
          const int call = score_tap_calls++;
          const char *dump_call = getenv ("CHICAGO_DUMP_CANDIDATE_CALL");

          printf ("candidate-config[%d]", call);
          for (int index = 0; index < 8; index++)
            printf (" %d", config[index]);
          printf ("\n");
          if (dump_call && atoi (dump_call) == call)
            {
              FILE *old_file = fopen ("z:/tmp/chicago-candidate-old.bin", "wb");
              FILE *new_file = fopen ("z:/tmp/chicago-candidate-new.bin", "wb");

              if (old_file)
                {
                  fwrite ((const void *) (uintptr_t) registers->Rcx, 0x3c,
                          config[3], old_file);
                  fclose (old_file);
                }
              if (new_file)
                {
                  fwrite ((const void *) (uintptr_t) registers->Rdx, 0x3c,
                          config[5], new_file);
                  fclose (new_file);
                }
            }
        }
      else if (score_tap_offset == 0x5a7b0)
        {
          const int32_t *stack =
            (const int32_t *) (uintptr_t) registers->Rsp;
          const int32_t *config = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x30);
          const int call = score_tap_calls++;
          const char *dump_call = getenv ("CHICAGO_DUMP_SELECTOR_CALL");

          printf ("selector-config[%d] caller=0x%llx count=%d limit=%d "
                  "config=%d,%d,%d,%d\n",
                  call,
                  (unsigned long long) (*(const uintptr_t *)
                    (uintptr_t) registers->Rsp -
                    ((uintptr_t) score_tap - score_tap_offset)),
                  (int32_t) registers->R9,
                  stack[0x28 / 4], config[0], config[1], config[2], config[3]);
          if (dump_call && atoi (dump_call) == call)
            {
              FILE *best_file = fopen ("z:/tmp/chicago-selector-best.bin", "wb");
              FILE *indices_file =
                fopen ("z:/tmp/chicago-selector-indices.bin", "wb");

              if (best_file)
                {
                  fwrite ((const void *) (uintptr_t) registers->Rdx,
                          sizeof (int32_t) * 2, registers->R9, best_file);
                  fclose (best_file);
                }
              if (indices_file)
                {
                  fwrite ((const void *) (uintptr_t) registers->R8,
                          sizeof (int32_t) * 2, registers->R9, indices_file);
                  fclose (indices_file);
                }
            }
        }
      else if (score_tap_offset == 0x5caf0)
        {
          unsigned char *gallery =
            (unsigned char *) (uintptr_t) registers->Rcx;
          const int32_t *stack =
            (const int32_t *) (uintptr_t) registers->Rsp;
          const uint32_t subtemplate_count =
            readable_span (gallery, 0x30) ?
              *(const uint32_t *) (gallery + 0x24) : 0;
          const uint32_t relation_count =
            readable_span (gallery, 0x1c0) ?
              *(const uint32_t *) (gallery + 0x2c) : 0;
          const char *output_path =
            getenv ("CHICAGO_CAPACITY_PRE_GALLERY_RELATIONS_OUT");
          int32_t *probe_relations = *(int32_t **)
            (uintptr_t) (registers->Rsp + 0x28);
          const char *probe_output_path =
            getenv ("CHICAGO_CAPACITY_PRE_PROBE_RELATIONS_OUT");
          const char *force_unresolved =
            getenv ("CHICAGO_CAPACITY_FORCE_UNRESOLVED_INDEX");

          printf ("capacity-refine-entry[%d] count=%u mode=%d\n",
                  score_tap_calls++, relation_count, stack[0x30 / 4]);
          if (force_unresolved)
            {
              const uint32_t target =
                (uint32_t) strtoul (force_unresolved, NULL, 0);
              unsigned char **subtemplates =
                (unsigned char **) (gallery + 0x30);

              if (target >= subtemplate_count ||
                  !readable_span (subtemplates,
                                  subtemplate_count * sizeof (*subtemplates)) ||
                  !readable_span (probe_relations,
                                  subtemplate_count * 0x1c))
                {
                  fprintf (stderr, "cannot force unresolved capacity index %u\n",
                           target);
                  ExitProcess (90);
                }
              for (uint32_t other = 0; other < subtemplate_count; other++)
                {
                  const uint32_t larger = target > other ? target : other;
                  const uint32_t smaller = target > other ? other : target;
                  uint32_t relation_index;
                  int32_t *relation;

                  if (other == target ||
                      !readable_span (subtemplates[larger], 0x108))
                    continue;
                  relation_index =
                    *(const uint32_t *) (subtemplates[larger] + 0x104) +
                    smaller;
                  if (relation_index >= relation_count)
                    continue;
                  relation = (int32_t *)
                    (gallery + 0x1c0 + (size_t) relation_index * 0x1c);
                  relation[0] = 0;
                }
              /* Zero is the retryable/unknown marker here.  The post-walk
               * test at +0x5cf88 deliberately treats -2 as resolved and
               * skips the expensive feature-rematching fallback. */
              probe_relations[target * 7] = 0;
              memset (probe_relations + target * 7 + 1, 0, 6 * sizeof (int32_t));
              probe_relations[target * 7 + 1] = 0x100;
              probe_relations[target * 7 + 5] = 0x100;
              printf ("capacity-forced-unresolved index=%u disconnected-edges=%u\n",
                      target, subtemplate_count - 1);
            }
          if (output_path && relation_count != 0 &&
              readable_span (gallery + 0x1c0,
                             (size_t) relation_count * 0x1c))
            {
              FILE *output = fopen (output_path, "wb");

              if (output)
                {
                  fwrite (gallery + 0x1c0, 0x1c, relation_count, output);
                  fclose (output);
              }
            }
          if (getenv ("CHICAGO_CAPACITY_CHAIN_TAP_OFFSET"))
            {
              score_tap_next_offset = strtoul (
                getenv ("CHICAGO_CAPACITY_CHAIN_TAP_OFFSET"), NULL, 0);
              score_tap_next = score_tap - score_tap_offset +
                               score_tap_next_offset;
            }
          if (probe_output_path && readable_span (probe_relations, 50 * 0x1c))
            {
              FILE *output = fopen (probe_output_path, "wb");

              if (output)
                {
                  fwrite (probe_relations, 0x1c, 50, output);
                  fclose (output);
                }
            }
        }
      else if (score_tap_offset == 0x5c993 ||
               score_tap_offset == 0x5cf2d)
        {
          const int32_t *stack =
            (const int32_t *) (uintptr_t) registers->Rsp;
          const int32_t *transform = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x38);
          const int32_t *strength = *(const int32_t **)
            (uintptr_t) (registers->Rsp + 0x40);

          printf ("capacity-%s-write[%d] from=%d to=%d mode=%d "
                  "strength=%d transform=%d,%d,%d,%d,%d,%d\n",
                  score_tap_offset == 0x5cf2d ? "propagate" : "closure",
                  score_tap_calls++, stack[0x20 / 4], stack[0x28 / 4],
                  stack[0x30 / 4], strength ? *strength : -999,
                  transform ? transform[0] : 0,
                  transform ? transform[1] : 0,
                  transform ? transform[2] : 0,
                  transform ? transform[3] : 0,
                  transform ? transform[4] : 0,
                  transform ? transform[5] : 0);
        }
      else if (score_tap_offset == 0x5aad1)
        {
          const int32_t *pairs =
            (const int32_t *) (uintptr_t) registers->Rbx;

          printf ("selector-output[%d] selected=%d limit=%d",
                  score_tap_calls++, (int32_t) registers->R10,
                  (int32_t) registers->Rdi);
          for (int index = 0; index < (int32_t) registers->Rdi; index++)
            printf (" %d:%d", pairs[index * 2], pairs[index * 2 + 1]);
          printf ("\n");
        }
      else if (score_tap_offset == 0x25cf8)
        {
          const int32_t *stack =
            (const int32_t *) (uintptr_t) registers->Rsp;

          printf ("scheduler-helper-return[%d] out6=%d out7=%d\n",
                  score_tap_calls++,
                  **(const int32_t **) (uintptr_t) (registers->Rsp + 0x58),
                  **(const int32_t **) (uintptr_t) (registers->Rsp + 0x60));
        }
      else if (score_tap_offset == 0x5d674)
        {
          const int32_t *stack =
            (const int32_t *) (uintptr_t) registers->Rsp;

          printf ("capacity-selector threshold=%d scores=",
                  stack[0x140 / 4]);
          for (int index = 0; index < 50; index++)
            printf ("%s%d", index ? "," : "", stack[0x144 / 4 + index]);
          printf (" secondary=");
          for (int index = 0; index < 50; index++)
            printf ("%s%d", index ? "," : "", stack[0x214 / 4 + index]);
          printf ("\n");
          if (readable_span ((const void *) (uintptr_t) registers->Rbx,
                             50 * 0x1c))
            {
              const int32_t *relations =
                (const int32_t *) (uintptr_t) registers->Rbx;
              const char *relations_output =
                getenv ("CHICAGO_CAPACITY_RELATIONS_OUT");

              if (relations_output)
                {
                  FILE *output = fopen (relations_output, "wb");

                  if (output)
                    {
                      fwrite (relations, 0x1c, 50, output);
                      fclose (output);
                    }
                }

              for (int index = 0; index < 50; index++)
                {
                  const int32_t *relation = relations + index * 7;

                  printf ("capacity-relation[%d]=%d/%d,%d,%d,%d,%d,%d\n",
                          index, relation[0], relation[1], relation[2],
                          relation[3], relation[4], relation[5], relation[6]);
                }
            }
          if (readable_span ((const void *) (uintptr_t) registers->R15,
                             0x1c0))
            {
              const unsigned char *gallery =
                (const unsigned char *) (uintptr_t) registers->R15;
              const uint32_t relation_count =
                *(const uint32_t *) (gallery + 0x2c);
              const char *gallery_relations_output =
                getenv ("CHICAGO_CAPACITY_GALLERY_RELATIONS_OUT");

              if (gallery_relations_output &&
                  readable_span (gallery + 0x1c0,
                                 (size_t) relation_count * 0x1c))
                {
                  FILE *output = fopen (gallery_relations_output, "wb");

                  if (output)
                    {
                      fwrite (gallery + 0x1c0, 0x1c, relation_count, output);
                      fclose (output);
                    }
                }
            }
        }
      else if (score_tap_offset == 0x5d030)
        {
          const int32_t *frame =
            (const int32_t *) (uintptr_t) registers->Rbp;
          const int32_t index = (int32_t) registers->R13;
          const int32_t *relation =
            (const int32_t *) (uintptr_t) registers->R14;

          printf ("capacity-fallback-scan[%d] index=%d resolved=%d relation=%d\n",
                  score_tap_calls++, index, frame[0x70 / 4 + index],
                  relation ? relation[0] : -999);
        }
      else if (score_tap_offset == 0x5d03c)
        {
          const int32_t index = (int32_t) registers->R13;
          const int32_t *relation =
            (const int32_t *) (uintptr_t) registers->R14;

          printf ("capacity-fallback-enter[%d] index=%d relation=%d\n",
                  score_tap_calls++, index,
                  relation ? relation[0] : -999);
        }
      else if (score_tap_offset == 0x5d1b0)
        {
          const int32_t *frame =
            (const int32_t *) (uintptr_t) registers->Rbp;
          const char *dump_index =
            getenv ("CHICAGO_DUMP_CAPACITY_FALLBACK_INDEX");

          printf ("capacity-fallback-pairs[%d] index=%d pairs=%d\n",
                  score_tap_calls++, (int32_t) registers->R12,
                  (int32_t) registers->Rdi);
          if (dump_index && atoi (dump_index) == (int32_t) registers->R12)
            {
              printf ("capacity-fallback-pair-vector index=%d",
                      (int32_t) registers->R12);
              for (int index = 0; index < (int32_t) registers->Rdi; index++)
                printf (" %d:%d", frame[0x610 / 4 + index * 2],
                        frame[0x614 / 4 + index * 2]);
              printf ("\n");
            }
        }
      else if (score_tap_offset == 0x5d2fd)
        {
          printf ("capacity-fallback-evidence[%d] index=%d inliers=%d "
                  "selector=%d metric=%d\n",
                  score_tap_calls++, (int32_t) registers->R12,
                  (int32_t) registers->Rbx, (int32_t) registers->Rcx,
                  (int32_t) registers->Rax);
        }
      else if (score_tap_offset == 0x5d33e ||
               score_tap_offset == 0x5d364)
        {
          printf ("capacity-fallback-%s[%d] index=%d inliers=%d\n",
                  score_tap_offset == 0x5d364 ? "accept" : "reject",
                  score_tap_calls++, (int32_t) registers->R12,
                  (int32_t) registers->Rbx);
        }
      else if (score_tap_offset == 0x2a6ac ||
               score_tap_offset == 0x2a834 ||
               score_tap_offset == 0x2a944 ||
               score_tap_offset == 0x2a9f7 ||
               score_tap_offset == 0x2aa67 ||
               score_tap_offset == 0x2ae3d)
        {
          const int32_t *geometry =
            (const int32_t *) (uintptr_t) (registers->Rbp + 0x310);
          const int32_t *entry = *(const int32_t **)
            (uintptr_t) (registers->Rbp + 0x48);
          const unsigned char *state = *(const unsigned char **)
            (uintptr_t) (registers->Rbp - 0x70);
          const int32_t *first_entry = (const int32_t *) state;

          printf ("study-relation-tap+0x%lx[%d] geometry=%d,%d,%d,%d,%d,%d "
                  "entry=%d,%d,%d,%d,%d,%d,%d,%d "
                  "first=%d,%d,%d,%d,%d,%d,%d,%d selected=%d\n",
                  score_tap_offset, score_tap_calls++,
                  geometry[0], geometry[1], geometry[2], geometry[3],
                  geometry[4], geometry[5],
                  entry ? entry[0] : -999, entry ? entry[1] : -999,
                  entry ? entry[2] : -999, entry ? entry[3] : -999,
                  entry ? entry[4] : -999, entry ? entry[5] : -999,
                  entry ? entry[6] : -999, entry ? entry[7] : -999,
                  first_entry[0], first_entry[1], first_entry[2],
                  first_entry[3], first_entry[4], first_entry[5],
                  first_entry[6], first_entry[7],
                  *(const int32_t *) (state + 0x648));
        }
      else
        {
          const int32_t *frame =
            (const int32_t *) (uintptr_t) registers->Rbp;
          const int32_t *stack =
            (const int32_t *) (uintptr_t) registers->Rsp;

          printf ("score-tap+0x%lx eax=%d ebx=%d ecx=%d edx=%d "
                  "r8=%d r9=%d r10=%d r11=%d r12=%d r13=%d r14=%d r15=%d",
                  score_tap_offset,
                  (int32_t) registers->Rax, (int32_t) registers->Rbx,
                  (int32_t) registers->Rcx, (int32_t) registers->Rdx,
                  (int32_t) registers->R8, (int32_t) registers->R9,
                  (int32_t) registers->R10, (int32_t) registers->R11,
                  (int32_t) registers->R12, (int32_t) registers->R13,
                  (int32_t) registers->R14, (int32_t) registers->R15);
          for (int offset = -0x80; offset <= -4; offset += 4)
            printf (" %d=%d", offset, frame[offset / 4]);
          for (int offset = 0; offset <= 0x40; offset += 4)
            printf (" +%02x=%d", offset, frame[offset / 4]);
          for (int offset = 0x60; offset <= 0x7c; offset += 4)
            printf (" sp+%02x=%d", offset, stack[offset / 4]);
          printf ("\n");
        }
      VirtualProtect (score_tap, 1, PAGE_EXECUTE_READWRITE, &protection);
      *score_tap = score_tap_original;
      FlushInstructionCache (GetCurrentProcess (), score_tap, 1);
      registers->Rip = (DWORD64) (uintptr_t) score_tap;
      registers->EFlags |= 0x100;
      score_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      aux_return_tap_rearm)
    {
      aux_return_tap_rearm = 0;
      registers->EFlags &= ~0x100;
      VirtualProtect (aux_return_tap, 1, PAGE_EXECUTE_READWRITE, &protection);
      *aux_return_tap = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), aux_return_tap, 1);
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      score_tap_rearm)
    {
      score_tap_rearm = 0;
      registers->EFlags &= ~0x100;
      if (score_tap_next)
        {
          score_tap = score_tap_next;
          score_tap_offset = score_tap_next_offset;
          score_tap_original = *score_tap;
          score_tap_next = NULL;
        }
      VirtualProtect (score_tap, 1, PAGE_EXECUTE_READWRITE, &protection);
      *score_tap = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), score_tap, 1);
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  return EXCEPTION_CONTINUE_SEARCH;
}

static unsigned char *
read_file (const char *path,
           size_t     *size)
{
  FILE *input = fopen (path, "rb");
  unsigned char *contents;
  long length;

  if (!input)
    return NULL;
  if (fseek (input, 0, SEEK_END) != 0 || (length = ftell (input)) <= 0 ||
      fseek (input, 0, SEEK_SET) != 0)
    {
      fclose (input);
      return NULL;
    }
  contents = malloc ((size_t) length);
  if (!contents || fread (contents, 1, (size_t) length, input) !=
                   (size_t) length)
    {
      free (contents);
      fclose (input);
      return NULL;
    }
  fclose (input);
  *size = (size_t) length;
  return contents;
}

static int
write_file (const char *path,
            const void *contents,
            size_t      size)
{
  FILE *output = fopen (path, "wb");

  if (!output)
    return 0;
  if (fwrite (contents, 1, size, output) != size)
    {
      fclose (output);
      return 0;
    }
  return fclose (output) == 0;
}

static int
readable_span (const void *address,
               size_t      length)
{
  MEMORY_BASIC_INFORMATION region;
  uintptr_t start = (uintptr_t) address;
  uintptr_t end;

  if (!address || length == 0 || start > UINTPTR_MAX - length)
    return 0;
  end = start + length;
  if (VirtualQuery (address, &region, sizeof (region)) != sizeof (region))
    return 0;
  if (region.State != MEM_COMMIT ||
      (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
    return 0;
  return end <= (uintptr_t) region.BaseAddress + region.RegionSize;
}

static void
dump_subtemplate_state (const char *kind,
                        uint32_t    index,
                        const void *subtemplate)
{
  const unsigned char *bytes = subtemplate;
  const unsigned char *derived_map = NULL;

  if (!readable_span (bytes, 0x164))
    {
      printf ("%s-sub[%u] unreadable=%p\n", kind, index, subtemplate);
      return;
    }

  memcpy (&derived_map, bytes + 0x148, sizeof (derived_map));
  printf ("%s-sub[%u] ptr=%p f0=%d v108=%d v10c=%d v110=%d "
          "v114=%d v118=%d v11c=%d v120=%d v124=%d "
          "c7=%d map148=%p v158=%d v15c=%d v160=%d",
          kind, index, subtemplate,
          *(const int32_t *) (bytes + 0xf0),
          *(const int32_t *) (bytes + 0x108),
          *(const int32_t *) (bytes + 0x10c),
          *(const int32_t *) (bytes + 0x110),
          *(const int32_t *) (bytes + 0x114),
          *(const int32_t *) (bytes + 0x118),
          *(const int32_t *) (bytes + 0x11c),
          *(const int32_t *) (bytes + 0x120),
          *(const int32_t *) (bytes + 0x124),
          *(const int32_t *) (bytes + 0x140), derived_map,
          *(const int32_t *) (bytes + 0x158),
          *(const int32_t *) (bytes + 0x15c),
          *(const int32_t *) (bytes + 0x160));
  if (readable_span (derived_map, 16))
    {
      printf (" prefix=");
      for (int byte = 0; byte < 16; byte++)
        printf ("%s%u", byte ? "," : "", derived_map[byte]);
    }
  else
    printf (" prefix=unreadable");
  printf ("\n");
}

int
main (int argc,
      char **argv)
{
  HMODULE chicago;
  template_unpack_t unpack;
  template_delete_t delete_template;
  identify_template_t identify_template;
  identify_score_t identify_score;
  study_mutate_t study_mutate;
  template_packed_size_t template_packed_size;
  template_pack_t template_pack;
  unsigned char *gallery_packed;
  unsigned char *probe_packed;
  void *gallery = NULL;
  void *probe = NULL;
  void *gallery_inner;
  void *probe_inner;
  FILE *vectors = NULL;
  size_t gallery_size;
  size_t probe_size;
  uint32_t probe_count;
  score_vector_t study_vector;
  void *study_probe = NULL;
  int have_study_vector = 0;
  int match_index = -999;
  int result;

  if (argc < 3)
    {
      fprintf (stderr, "usage: %s gallery.bin probe.bin [vectors.bin]\n",
               argv[0]);
      return 2;
    }
  gallery_packed = read_file (argv[1], &gallery_size);
  probe_packed = read_file (argv[2], &probe_size);
  if (!gallery_packed || !probe_packed)
    {
      fprintf (stderr, "could not read packed template input\n");
      return 3;
    }
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    {
      fprintf (stderr, "could not load AlgoChicago.dll: %lu\n",
               GetLastError ());
      return 4;
    }
  unpack = (template_unpack_t) GetProcAddress (chicago, "templateUnPack");
  delete_template =
    (template_delete_t) GetProcAddress (chicago, "templateDelete");
  identify_template =
    (identify_template_t) GetProcAddress (chicago, "identifytemplate");
  identify_score = (identify_score_t) ((unsigned char *) chicago + 0x290f0);
  study_mutate = (study_mutate_t) ((unsigned char *) chicago + 0x5dd60);
  template_packed_size = (template_packed_size_t)
    GetProcAddress (chicago, "templateGetPackedSize");
  template_pack = (template_pack_t)
    GetProcAddress (chicago, "templatePack");
  if (getenv ("CHICAGO_SCORE_TRACE"))
    {
      DWORD protection;

      score_tap_offset = getenv ("CHICAGO_TAP_OFFSET") ?
        strtoul (getenv ("CHICAGO_TAP_OFFSET"), NULL, 0) : 0x28960;
      score_tap = (unsigned char *) chicago + score_tap_offset;
      score_tap_original = *score_tap;
      AddVectoredExceptionHandler (1, score_tap_handler);
      VirtualProtect (score_tap, 1, PAGE_EXECUTE_READWRITE, &protection);
      *score_tap = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), score_tap, 1);
      if (getenv ("CHICAGO_AUX_COUNT_VECTOR"))
        {
          aux_count_vectors =
            fopen (getenv ("CHICAGO_AUX_COUNT_VECTOR"), "wb");
          if (!aux_count_vectors)
            return 5;
          aux_return_tap = (unsigned char *) chicago + 0x5143e;
          aux_return_tap_original = *aux_return_tap;
          VirtualProtect (aux_return_tap, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          *aux_return_tap = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), aux_return_tap, 1);
        }
    }
  if (!unpack || !delete_template || !identify_template)
    {
      fprintf (stderr, "missing AlgoChicago export\n");
      return 5;
    }
  result = unpack (gallery_packed, (int) gallery_size, NULL, &gallery);
  printf ("gallery unpack rc=0x%x handle=%p\n", result, gallery);
  if (result != 0 || !gallery)
    return 6;
  result = unpack (probe_packed, (int) probe_size, NULL, &probe);
  printf ("probe unpack rc=0x%x handle=%p\n", result, probe);
  if (result != 0 || !probe)
    return 7;

  gallery_inner = *(void **) gallery;
  probe_inner = *(void **) probe;
  probe_count = *(uint32_t *) ((unsigned char *) probe_inner + 0x24);
  if (getenv ("CHICAGO_GALLERY_FIRST_INDEX"))
    {
      uint32_t requested = (uint32_t) strtoul (
        getenv ("CHICAGO_GALLERY_FIRST_INDEX"), NULL, 0);
      uint32_t *schedule = (uint32_t *) ((unsigned char *) gallery_inner +
                                        0x87f0);
      uint32_t gallery_count = *(uint32_t *) ((unsigned char *) gallery_inner +
                                             0x24);

      if (requested >= gallery_count)
        return 8;
      for (uint32_t index = 0; index < gallery_count; index++)
        if (schedule[index] == requested)
          {
            uint32_t first = schedule[0];

            schedule[0] = requested;
            schedule[index] = first;
            break;
          }
    }
  if (getenv ("CHICAGO_FORCE_AUX_PREFIX"))
    {
      const unsigned prefix =
        strtoul (getenv ("CHICAGO_FORCE_AUX_PREFIX"), NULL, 0);

      for (uint32_t index = 0; index < probe_count; index++)
        {
          unsigned char *probe_subtemplate = *(unsigned char **)
            ((unsigned char *) probe_inner + 0x30 + index * 8);
          unsigned char *map = calloc (1, 40 * 32);

          if (!map)
            return 8;
          map[0] = (unsigned char) prefix;
          map[3] = 4;
          *(unsigned char **) (probe_subtemplate + 0x148) = map;
        }
    }
  printf ("gallery inner=%p count=%u probe inner=%p count=%u\n",
          gallery_inner,
          *(uint32_t *) ((unsigned char *) gallery_inner + 0x24),
          probe_inner, probe_count);
  for (uint32_t index = 0;
       index < *(uint32_t *) ((unsigned char *) gallery_inner + 0x24);
       index++)
    dump_subtemplate_state (
      "gallery", index,
      *(void **) ((unsigned char *) gallery_inner + 0x30 + index * 8));
  for (uint32_t index = 0; index < probe_count; index++)
    dump_subtemplate_state (
      "probe", index,
      *(void **) ((unsigned char *) probe_inner + 0x30 + index * 8));
  if (argc > 3)
    vectors = fopen (argv[3], "wb");

  for (uint32_t index = 0; index < probe_count; index++)
    {
      score_vector_t vector;
      void *probe_subtemplate =
        *(void **) ((unsigned char *) probe_inner + 0x30 + index * 8);

      if (getenv ("CHICAGO_STUDY_ONLY_PROBE") &&
          index != (getenv ("CHICAGO_STUDY_PROBE_INDEX") ?
                    strtoul (getenv ("CHICAGO_STUDY_PROBE_INDEX"), NULL, 0) :
                    0))
        continue;

      memset (&vector, 0, sizeof (vector));
      vector.score = 0;
      vector.return_code = identify_score (&vector.score, probe_subtemplate,
                                            gallery_inner,
                                            getenv ("CHICAGO_SCORE_INDEX") ?
                                              strtol (getenv ("CHICAGO_SCORE_INDEX"),
                                                      NULL, 0) :
                                              (int) index,
                                            0,
                                            vector.scratch, vector.detail);
      printf ("sub=%u rc=0x%x score=%d detail=%d,%d,%d,%d,%d,%d "
              "scratch0=%d,%d,%d,%d,%d,%d,%d,%d "
              "scratch648=%d 68c=%d 690=%d\n",
              index, vector.return_code, vector.score,
              vector.detail[0], vector.detail[1], vector.detail[2],
              vector.detail[3], vector.detail[4], vector.detail[5],
              ((int32_t *) vector.scratch)[0],
              ((int32_t *) vector.scratch)[1],
              ((int32_t *) vector.scratch)[2],
              ((int32_t *) vector.scratch)[3],
              ((int32_t *) vector.scratch)[4],
              ((int32_t *) vector.scratch)[5],
              ((int32_t *) vector.scratch)[6],
              ((int32_t *) vector.scratch)[7],
              *(int32_t *) (vector.scratch + 0x648),
              *(int32_t *) (vector.scratch + 0x68c),
              *(int32_t *) (vector.scratch + 0x690));
      if (vectors)
        fwrite (&vector, sizeof (vector), 1, vectors);
      if (!have_study_vector &&
          index == (getenv ("CHICAGO_STUDY_PROBE_INDEX") ?
                    strtoul (getenv ("CHICAGO_STUDY_PROBE_INDEX"), NULL, 0) :
                    0))
        {
          study_vector = vector;
          study_probe = probe_subtemplate;
          have_study_vector = 1;
          if (getenv ("CHICAGO_STUDY_APPEND_OUT"))
            break;
        }
    }
  if (vectors)
    fclose (vectors);
  if (aux_count_vectors)
    {
      fclose (aux_count_vectors);
      aux_count_vectors = NULL;
    }

  if (!getenv ("CHICAGO_STUDY_APPEND_OUT"))
    {
      result = identify_template (gallery, probe, NULL, &match_index);
      printf ("identifytemplate rc=0x%x index=%d %s\n", result, match_index,
              match_index >= 0 ? "MATCH" : "no-match");
    }
  else
    {
      const char *output_path = getenv ("CHICAGO_STUDY_APPEND_OUT");
      int selected_index = getenv ("CHICAGO_STUDY_SELECTED_INDEX") ?
        strtol (getenv ("CHICAGO_STUDY_SELECTED_INDEX"), NULL, 0) :
        *(int32_t *) (study_vector.scratch + 0x648);
      int update_status = 0;
      int appended_index;
      int packed_size;
      unsigned char *packed;

      if (!have_study_vector || !study_probe || !study_mutate ||
          !template_packed_size || !template_pack)
        {
          fprintf (stderr, "study append prerequisites are unavailable\n");
          return 9;
        }
      *(int32_t *) (study_vector.scratch + 0x648) = selected_index;
      *(int32_t *) (study_vector.scratch + 0x688) = 1;
      appended_index = study_mutate (gallery_inner, study_probe,
                                     study_vector.scratch, &update_status,
                                     getenv ("CHICAGO_STUDY_ALLOW_REPLACE") ?
                                       strtol (getenv (
                                         "CHICAGO_STUDY_ALLOW_REPLACE"),
                                               NULL, 0) :
                                       0);
      printf ("study-append selected=%d returned=%d update=%d count=%u\n",
              selected_index, appended_index, update_status,
              *(uint32_t *) ((unsigned char *) gallery_inner + 0x24));
      packed_size = template_packed_size (gallery);
      if (packed_size <= 0)
        return 10;
      packed = calloc (1, (size_t) packed_size);
      if (!packed || template_pack (gallery, packed) != 0 ||
          !write_file (output_path, packed, (size_t) packed_size))
        return 11;
      printf ("study-append packed=%d output=%s\n", packed_size, output_path);
      free (packed);
    }
  delete_template (probe);
  delete_template (gallery);
  free (probe_packed);
  free (gallery_packed);
  return 0;
}
