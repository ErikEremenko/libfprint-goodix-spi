// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Sequential production AlgoChicago enrollment-state oracle. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64
#define HEIGHT 80
#define PIXELS (WIDTH * HEIGHT)
#define RAW_BYTES (PIXELS * 2)
#define HEADER_BYTES 0x40

#define PTR(p, off, value) (*(void **) ((unsigned char *) (p) + (off)) = (void *) (value))
#define U16(p, off, value) (*(uint16_t *) ((unsigned char *) (p) + (off)) = (uint16_t) (value))
#define U32(p, off, value) (*(uint32_t *) ((unsigned char *) (p) + (off)) = (uint32_t) (value))
#define U8(p, off, value) (*(uint8_t *) ((unsigned char *) (p) + (off)) = (uint8_t) (value))
#define EXPORT(module, type, name) ((type) (void *) GetProcAddress ((module), (name)))

typedef int (*ppp_param_init_t) (int);
typedef int (*preprocess_init_calidata_t) (void);
typedef unsigned (*preprocess_get_calidata_len_t) (void);
typedef int (*preprocess_load_calidata_t) (void *, unsigned);
typedef int (*preprocessor_init_t) (void *);
typedef int (*preprocess_set_mode_t) (int);
typedef int (*preprocessor_t) (void *, void *, void *, void *, void *, char, char);
typedef void *(*enrol_start_t) (void);
typedef int (*enrol_add_image_t) (void *, void *, void *, void *, char, void *);
typedef int (*enrol_get_template_t) (void *, void **);
typedef int (*template_get_packed_size_t) (void *);
typedef int (*template_pack_t) (void *, void *);

typedef struct
{
  uint32_t add_return;
  uint32_t required;
  uint32_t accepted;
  uint32_t progress;
  uint32_t position_x;
  uint32_t position_y;
  uint32_t subtemplate_count;
  uint32_t capacity;
  uint32_t transform_count;
  uint32_t record_count;
  uint32_t active_count;
  uint32_t quality;
  uint32_t coverage;
  uint32_t state_100;
  uint32_t state_104;
  uint32_t reserved;
} EnrollmentStep;

typedef struct
{
  uint32_t sequence;
  uint32_t relation_count;
  uint32_t input_index;
  uint32_t input_threshold;
  uint32_t input_detail;
  uint32_t input_weight;
  uint32_t enrollment_count;
  uint32_t enrollment_capacity;
  uint32_t transform_count;
  uint32_t reserved[7];
} RelationTapHeader;

static unsigned char *relation_tap_address;
static unsigned char relation_tap_original;
static int relation_tap_rearm;
static uint32_t relation_tap_sequence;
static FILE *relation_tap_output;
static unsigned char *relation_tap_return_address;
static unsigned char relation_tap_return_original;
static unsigned char *relation_tap_input;
static unsigned char *relation_tap_enrollment;
static unsigned char *group_tap_address;
static unsigned char group_tap_original;
static int group_tap_rearm;

typedef struct
{
  uint32_t relation_sequence;
  uint32_t call_index;
  uint32_t return_rva;
  uint32_t reserved;
  int32_t dimensions[4];
  int32_t transform[6];
  int32_t result;
  int32_t scratch[4];
} OverlapTapRecord;

static unsigned char *overlap_tap_address;
static unsigned char overlap_tap_original;
static unsigned char *overlap_tap_return_address;
static unsigned char overlap_tap_return_original;
static int overlap_tap_rearm;
static uint32_t overlap_tap_call_index;
static FILE *overlap_tap_output;
static OverlapTapRecord overlap_tap_record;
static int32_t *overlap_tap_scratch;
static unsigned char *chicago_base;
static unsigned char *adapt_trace_address;
static unsigned char adapt_trace_original;
static unsigned char *adapt_mask_address;
static unsigned char adapt_mask_original;
static unsigned char *adapt_core_address;
static unsigned char adapt_core_original;
static unsigned char *adapt_stage_addresses[3];
static unsigned char adapt_stage_originals[3];
static unsigned char *adapt_inputs_address;
static unsigned char adapt_inputs_original;
static unsigned char *adapt_normalized_address;
static unsigned char adapt_normalized_original;
static unsigned char *adapt_normalized_post_address;
static unsigned char adapt_normalized_post_original;
static unsigned char *adapt_update_address;
static unsigned char adapt_update_original;
static unsigned char *adapt_update_return_address;
static unsigned char adapt_update_return_original;
static uintptr_t adapt_update_arguments[7];
static unsigned char *adapt_update_second_address;
static unsigned char adapt_update_second_original;
static unsigned char *adapt_update_second_return_address;
static unsigned char adapt_update_second_return_original;
static uintptr_t adapt_update_second_arguments[7];
static unsigned char *adapt_average_address;
static unsigned char adapt_average_original;
static unsigned char *adapt_average_return_address;
static unsigned char adapt_average_return_original;
static uintptr_t adapt_average_arguments[4];
static unsigned char *adapt_filter_address;
static unsigned char adapt_filter_original;
static unsigned char *adapt_filter_core_address;
static unsigned char adapt_filter_core_original;
static unsigned char *adapt_generic_filter_address;
static unsigned char adapt_generic_filter_original;
static unsigned char *adapt_generic_filter_return_address;
static unsigned char adapt_generic_filter_return_original;
static int adapt_trace_step;
static int adapt_trace_handler_installed;
static unsigned char *resolution_tap_address;
static unsigned char resolution_tap_original;
static int resolution_tap_rearm;
static unsigned char *resolution_class_tap_address;
static unsigned char resolution_class_tap_original;
static int resolution_class_tap_rearm;
static unsigned char *resolution_input_tap_address;
static unsigned char resolution_input_tap_original;
static int resolution_input_tap_rearm;
static unsigned char *resolution_code_tap_address;
static unsigned char resolution_code_tap_original;
static int resolution_code_tap_rearm;
static unsigned char *resolution_context_tap_address;
static unsigned char resolution_context_tap_original;
static int resolution_context_tap_rearm;
static unsigned char *resolution_scratch_tap_address;
static unsigned char resolution_scratch_tap_original;
static int resolution_scratch_tap_rearm;
static unsigned char *resolution_labels_tap_address;
static unsigned char resolution_labels_tap_original;
static int resolution_labels_tap_rearm;
static unsigned resolution_labels_tap_sequence;
static unsigned char *resolution_stage_tap_addresses[13];
static unsigned char resolution_stage_tap_originals[13];
static int resolution_stage_tap_rearm = -1;
static unsigned resolution_stage_tap_sequences[13];

static LONG CALLBACK
resolution_tap_handler (EXCEPTION_POINTERS *exception)
{
  CONTEXT *context = exception->ContextRecord;
  DWORD protection;

  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      resolution_stage_tap_rearm >= 0)
    {
      const int stage = resolution_stage_tap_rearm;

      VirtualProtect (resolution_stage_tap_addresses[stage], 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_stage_tap_addresses[stage] = 0xcc;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_stage_tap_addresses[stage], 1);
      resolution_stage_tap_rearm = -1;
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      resolution_labels_tap_rearm)
    {
      VirtualProtect (resolution_labels_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_labels_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_labels_tap_address, 1);
      resolution_labels_tap_rearm = 0;
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      resolution_scratch_tap_rearm)
    {
      VirtualProtect (resolution_scratch_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_scratch_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_scratch_tap_address, 1);
      resolution_scratch_tap_rearm = 0;
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      resolution_context_tap_rearm)
    {
      VirtualProtect (resolution_context_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_context_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_context_tap_address, 1);
      resolution_context_tap_rearm = 0;
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      resolution_code_tap_rearm)
    {
      VirtualProtect (resolution_code_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_code_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), resolution_code_tap_address,
                             1);
      resolution_code_tap_rearm = 0;
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      resolution_input_tap_rearm)
    {
      VirtualProtect (resolution_input_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_input_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), resolution_input_tap_address,
                             1);
      resolution_input_tap_rearm = 0;
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      resolution_class_tap_rearm)
    {
      VirtualProtect (resolution_class_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_class_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), resolution_class_tap_address,
                             1);
      resolution_class_tap_rearm = 0;
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      resolution_tap_rearm)
    {
      VirtualProtect (resolution_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *resolution_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), resolution_tap_address, 1);
      resolution_tap_rearm = 0;
      context->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == resolution_context_tap_address)
    {
      const unsigned char *input =
        (const unsigned char *) (uintptr_t) context->Rcx;

      printf ("resolution-context=%u,%u,%u,%u,%u,%u\n",
              input ? input[0] : 0, input ? input[1] : 0,
              input ? input[2] : 0, input ? input[3] : 0,
              input ? input[4] : 0, input ? input[5] : 0);
      VirtualProtect (resolution_context_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_context_tap_address = resolution_context_tap_original;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_context_tap_address, 1);
      context->Rip = (DWORD64) (uintptr_t) resolution_context_tap_address;
      context->EFlags |= 0x100;
      resolution_context_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == resolution_scratch_tap_address)
    {
      const int *code = (const int *) (uintptr_t) context->Rdi;
      const int *covered = (const int *) (uintptr_t)
        *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x48);

      printf ("resolution-scratch stage=%u,%u,%u code=%u auxiliary=%u "
              "covered=%u pixels=%u ratio=%u\n",
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x50),
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x60),
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x54),
              code ? (unsigned) *code : 0,
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x58),
              covered ? (unsigned) *covered : 0,
              (unsigned) context->R12,
              covered && context->R12 ?
                ((unsigned) *covered * 100u + 50u) /
                  (unsigned) context->R12 : 0);
      VirtualProtect (resolution_scratch_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_scratch_tap_address = resolution_scratch_tap_original;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_scratch_tap_address, 1);
      context->Rip = (DWORD64) (uintptr_t) resolution_scratch_tap_address;
      context->EFlags |= 0x100;
      resolution_scratch_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == resolution_labels_tap_address)
    {
      const unsigned char *labels =
        (const unsigned char *) (uintptr_t) context->Rbx;
      const char *dump_dir = getenv ("CHICAGO_RESOLUTION_DUMP_DIR");
      unsigned counts[4] = { 0, 0, 0, 0 };

      for (unsigned index = 0; labels && index < (unsigned) context->R12;
           index++)
        if (labels[index] < 4)
          counts[labels[index]]++;
      if (dump_dir && *dump_dir && labels && context->R12)
        {
          char path[MAX_PATH];
          FILE *output;

          snprintf (path, sizeof (path), "%s/resolution-labels-%02u.bin",
                    dump_dir, resolution_labels_tap_sequence);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite (labels, 1, (size_t) context->R12, output);
              fclose (output);
            }
        }
      printf ("resolution-labels=%u,%u,%u,%u pixels=%u\n",
              counts[0], counts[1], counts[2], counts[3],
              (unsigned) context->R12);
      resolution_labels_tap_sequence++;
      VirtualProtect (resolution_labels_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_labels_tap_address = resolution_labels_tap_original;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_labels_tap_address, 1);
      context->Rip = (DWORD64) (uintptr_t) resolution_labels_tap_address;
      context->EFlags |= 0x100;
      resolution_labels_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    for (int stage = 0; stage < 13; stage++)
      if (exception->ExceptionRecord->ExceptionAddress ==
          resolution_stage_tap_addresses[stage])
        {
          const char *dump_dir = getenv ("CHICAGO_RESOLUTION_STAGE_DUMP_DIR");
          const char *stage_names[13] = {
            "38ae0", "3ac50", "38ae0-thresholds", "3ac50-thresholds",
            "prefinal", "threshold-output", "classifier-output",
            "exceptional-threshold-output", "exceptional-classifier-output",
            "generator-entry", "primary-builder-entry", "peak-entry",
            "peak-output",
          };

          if (stage == 2)
            {
              printf ("resolution-thresholds=38ae0 sequence=%u "
                      "secondary-hard=%u primary-high=%u primary-mid=%u "
                      "secondary-mid=%u primary-low=%u\n",
                      resolution_stage_tap_sequences[stage],
                      (unsigned) context->Rbx,
                      *(const unsigned *) (uintptr_t) (context->Rsp + 0x88),
                      (unsigned) context->Rbp, (unsigned) context->R14,
                      (unsigned) context->R13);
              printf ("resolution-promotion-threshold=38ae0 sequence=%u "
                      "value=%u\n", resolution_stage_tap_sequences[stage],
                      *(const unsigned *) (uintptr_t) (context->Rsp + 0x80));
            }
          else if (stage == 3)
            printf ("resolution-thresholds=3ac50 sequence=%u "
                    "secondary-hard=%u primary-high=%u primary-mid=%u "
                    "primary-low=%u\n",
                    resolution_stage_tap_sequences[stage],
                    (unsigned) context->R13, (unsigned) context->R12,
                    (unsigned) context->R15, (unsigned) context->R11);
          else if (stage == 10)
            {
              const size_t pixels = PIXELS;
              const void *planes[5] = {
                (const void *) (uintptr_t) context->Rcx,
                (const void *) (uintptr_t) context->Rdx,
                (const void *) (uintptr_t) context->R8,
                (const void *) (uintptr_t) context->R9,
                (const void *) (uintptr_t)
                  *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x28),
              };
              const size_t sizes[5] = {
                pixels * 2, pixels * 2, pixels * 2, pixels, pixels,
              };
              const char *names[5] = {
                "secondary16", "gradient16", "filtered-gradient16",
                "directions", "input-mask",
              };

              printf ("resolution-primary-builder-entry sequence=%u "
                      "secondary-threshold=%d gradient-threshold=%d "
                      "descending=%d\n",
                      resolution_stage_tap_sequences[stage],
                      *(const int *) (uintptr_t) (context->Rsp + 0x48),
                      *(const int *) (uintptr_t) (context->Rsp + 0x50),
                      *(const int *) (uintptr_t) (context->Rsp + 0x58));
              if (dump_dir && *dump_dir)
                for (unsigned plane = 0; plane < 5; plane++)
                  {
                    char path[MAX_PATH];
                    FILE *dump;

                    snprintf (path, sizeof (path),
                              "%s/resolution-primary-builder-entry-%02u-%s.bin",
                              dump_dir, resolution_stage_tap_sequences[stage],
                              names[plane]);
                    dump = fopen (path, "wb");
                    if (dump)
                      {
                        fwrite (planes[plane], 1, sizes[plane], dump);
                        fclose (dump);
                      }
                  }
            }
          else if (stage == 9)
            {
              const unsigned char *input16 =
                (const unsigned char *) (uintptr_t) context->Rdx;
              const unsigned char *mask =
                (const unsigned char *) (uintptr_t) context->R8;

              printf ("resolution-generator-entry sequence=%u mode=%u\n",
                      resolution_stage_tap_sequences[stage],
                      (unsigned) context->Rcx);
              if (dump_dir && *dump_dir && input16 && mask)
                {
                  char path[MAX_PATH];
                  FILE *dump;

                  snprintf (path, sizeof (path),
                            "%s/resolution-generator-entry-%02u-input16.bin",
                            dump_dir, resolution_stage_tap_sequences[stage]);
                  dump = fopen (path, "wb");
                  if (dump)
                    {
                      fwrite (input16, 2, PIXELS, dump);
                      fclose (dump);
                    }
                  snprintf (path, sizeof (path),
                            "%s/resolution-generator-entry-%02u-mask.bin",
                            dump_dir, resolution_stage_tap_sequences[stage]);
                  dump = fopen (path, "wb");
                  if (dump)
                    {
                      fwrite (mask, 1, PIXELS, dump);
                      fclose (dump);
                    }
                }
            }
          else if (stage == 11)
            {
              const int *histogram =
                (const int *) (uintptr_t) context->Rcx;

              printf ("resolution-peak-entry sequence=%u low=%d ratio=%d\n",
                      resolution_stage_tap_sequences[stage],
                      (int) context->Rdx, (int) context->R8);
              if (dump_dir && *dump_dir && histogram)
                {
                  char path[MAX_PATH];
                  FILE *dump;

                  snprintf (path, sizeof (path),
                            "%s/resolution-peak-entry-%02u-histogram.bin",
                            dump_dir, resolution_stage_tap_sequences[stage]);
                  dump = fopen (path, "wb");
                  if (dump)
                    {
                      fwrite (histogram, sizeof (*histogram), 400, dump);
                      fclose (dump);
                    }
                }
            }
          else if (stage == 12)
            printf ("resolution-peak-output sequence=%u state=%d peak=%d\n",
                    resolution_stage_tap_sequences[stage],
                    (int) context->Rax,
                    *(const int *) (uintptr_t) (context->Rsp + 0x50));
          else if (stage >= 4)
            {
              const uintptr_t output = stage == 4 ?
                (uintptr_t) context->Rdi :
                (stage == 6 ?
                  *(const uintptr_t *) ((uintptr_t) context->Rbp - 0x38) : 0);
              const unsigned char *labels = stage == 5 ?
                (const unsigned char *) (uintptr_t) context->Rbx :
                (stage >= 7 ?
                  (const unsigned char *) (uintptr_t)
                    *(const uintptr_t *) (uintptr_t) (context->Rsp + 0xa0) :
                (output ?
                (const unsigned char *) (uintptr_t)
                  *(const uintptr_t *) (output + 0x28) : NULL));

              printf ("resolution-%s sequence=%u\n", stage_names[stage],
                      resolution_stage_tap_sequences[stage]);
              if (stage == 5)
                printf ("resolution-promotion-threshold sequence=%u value=%u\n",
                        resolution_stage_tap_sequences[stage],
                        (unsigned) *(const unsigned short *)
                          (uintptr_t) (context->Rsp + 0xb0));
              if (dump_dir && *dump_dir && labels)
                {
                  char path[MAX_PATH];
                  FILE *dump;

                  snprintf (path, sizeof (path),
                            "%s/resolution-%s-%02u.bin", dump_dir,
                            stage_names[stage],
                            resolution_stage_tap_sequences[stage]);
                  dump = fopen (path, "wb");
                  if (dump)
                    {
                      fwrite (labels, 1, PIXELS, dump);
                      fclose (dump);
                    }
                }
            }
          else
            {
          const unsigned width =
            *(const unsigned *) (uintptr_t) (context->Rsp + 0x38);
          const unsigned height =
            *(const unsigned *) (uintptr_t) (context->Rsp + 0x40);
          const size_t pixels = (size_t) width * height;
          const void *planes[8] = {
            (const void *) (uintptr_t) context->Rcx,
            (const void *) (uintptr_t) context->Rdx,
            (const void *) (uintptr_t) context->R8,
            (const void *) (uintptr_t)
              *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x48),
            (const void *) (uintptr_t)
              *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x50),
            (const void *) (uintptr_t)
              *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x30),
            (const void *) (uintptr_t) context->R9,
            (const void *) (uintptr_t)
              *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x28),
          };
          const size_t sizes[8] = {
            pixels * 2, pixels * 2, pixels, pixels, pixels, 0x120, 0x120,
            0x120,
          };
          const char *names[8] = {
            "primary16", "secondary16", "input-mask", "labels-before",
            "class-mask", "config", "stats-r9", "stats-stack28",
          };

          printf ("resolution-stage=%s sequence=%u dims=%u/%u\n",
                  stage_names[stage],
                  resolution_stage_tap_sequences[stage], width, height);
          if (dump_dir && *dump_dir && pixels)
            for (unsigned plane = 0; plane < 8; plane++)
              if (planes[plane])
                {
                  char path[MAX_PATH];
                  FILE *output;

                  snprintf (path, sizeof (path),
                            "%s/resolution-stage-%s-%02u-%s.bin", dump_dir,
                            stage_names[stage],
                            resolution_stage_tap_sequences[stage],
                            names[plane]);
                  output = fopen (path, "wb");
                  if (output)
                    {
                      fwrite (planes[plane], 1, sizes[plane], output);
                      fclose (output);
                    }
                }
            }
          resolution_stage_tap_sequences[stage]++;
          VirtualProtect (resolution_stage_tap_addresses[stage], 1,
                          PAGE_EXECUTE_READWRITE, &protection);
          *resolution_stage_tap_addresses[stage] =
            resolution_stage_tap_originals[stage];
          FlushInstructionCache (GetCurrentProcess (),
                                 resolution_stage_tap_addresses[stage], 1);
          context->Rip = (DWORD64) (uintptr_t)
            resolution_stage_tap_addresses[stage];
          context->EFlags |= 0x100;
          resolution_stage_tap_rearm = stage;
          return EXCEPTION_CONTINUE_EXECUTION;
        }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == resolution_class_tap_address)
    {
      printf ("resolution-classes c1=%u c2=%u dark=%u initial=%u\n",
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x54),
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x50),
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x30),
              (unsigned) context->Rsi);
      VirtualProtect (resolution_class_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_class_tap_address = resolution_class_tap_original;
      FlushInstructionCache (GetCurrentProcess (), resolution_class_tap_address,
                             1);
      context->Rip = (DWORD64) (uintptr_t) resolution_class_tap_address;
      context->EFlags |= 0x100;
      resolution_class_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == resolution_input_tap_address)
    {
      const unsigned char *input =
        (const unsigned char *) (uintptr_t) context->Rcx;

      printf ("resolution-input width=%u height=%u bits=",
              (unsigned) context->R9, (unsigned) context->R8);
      for (unsigned index = 0; input && index < (unsigned) context->R9;
           index++)
        putchar ((input[index] & 1) ? '1' : '0');
      putchar ('\n');
      VirtualProtect (resolution_input_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_input_tap_address = resolution_input_tap_original;
      FlushInstructionCache (GetCurrentProcess (), resolution_input_tap_address,
                             1);
      context->Rip = (DWORD64) (uintptr_t) resolution_input_tap_address;
      context->EFlags |= 0x100;
      resolution_input_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == resolution_code_tap_address)
    {
      printf ("resolution-code=%u\n", (unsigned) context->Rcx);
      VirtualProtect (resolution_code_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_code_tap_address = resolution_code_tap_original;
      FlushInstructionCache (GetCurrentProcess (), resolution_code_tap_address,
                             1);
      context->Rip = (DWORD64) (uintptr_t) resolution_code_tap_address;
      context->EFlags |= 0x100;
      resolution_code_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT ||
      exception->ExceptionRecord->ExceptionAddress != resolution_tap_address)
    return EXCEPTION_CONTINUE_SEARCH;

  printf ("resolution-set step=%d low=%u scale=%u value=0x%x\n",
          adapt_trace_step, (unsigned) context->Rdx,
          (unsigned) context->R8,
          (unsigned) context->Rdx + ((unsigned) context->R8 << 8));
  VirtualProtect (resolution_tap_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *resolution_tap_address = resolution_tap_original;
  FlushInstructionCache (GetCurrentProcess (), resolution_tap_address, 1);
  context->Rip = (DWORD64) (uintptr_t) resolution_tap_address;
  context->EFlags |= 0x100;
  resolution_tap_rearm = 1;
  return EXCEPTION_CONTINUE_EXECUTION;
}

static LONG CALLBACK
adapt_trace_handler (EXCEPTION_POINTERS *exception)
{
  CONTEXT *context = exception->ContextRecord;
  unsigned char *hit;
  const char *prefix;
  char path[MAX_PATH];
  FILE *output;
  DWORD protection;

  if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
    return EXCEPTION_CONTINUE_SEARCH;
  hit = (unsigned char *) (uintptr_t) context->Rip;
  if (hit == adapt_generic_filter_return_address ||
      hit - 1 == adapt_generic_filter_return_address)
    {
      const uintptr_t object = *(const uintptr_t *) (uintptr_t)
        (context->Rsp + 0x50);
      const uintptr_t plane = object ?
        *(const uintptr_t *) (object + 0x18) : 0;
      const uintptr_t destination_object = *(const uintptr_t *) (uintptr_t)
        (context->Rsp + 0x58);
      const uintptr_t destination = destination_object ?
        *(const uintptr_t *) (destination_object + 0x18) : 0;

      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix && plane)
        {
          snprintf (path, sizeof (path), "%s-generic-filter-after-%02d.bin",
                    prefix, adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite ((const void *) plane, 1, RAW_BYTES, output);
              fclose (output);
            }
          if (destination)
            {
              snprintf (path, sizeof (path),
                        "%s-generic-filter-destination-after-%02d.bin",
                        prefix, adapt_trace_step);
              output = fopen (path, "wb");
              if (output)
                {
                  fwrite ((const void *) destination, 1, RAW_BYTES, output);
                  fclose (output);
                }
            }
        }
      hit = adapt_generic_filter_return_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_generic_filter_return_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_generic_filter_address ||
      hit - 1 == adapt_generic_filter_address)
    {
      const uintptr_t object = *(const uintptr_t *) (uintptr_t)
        (context->Rsp + 0x50);
      const uintptr_t plane = object ?
        *(const uintptr_t *) (object + 0x18) : 0;
      const uintptr_t destination_object = *(const uintptr_t *) (uintptr_t)
        (context->Rsp + 0x58);
      const uintptr_t destination = destination_object ?
        *(const uintptr_t *) (destination_object + 0x18) : 0;

      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      printf ("generic filter step=%d plane=%p scale=%u\n",
              adapt_trace_step, (void *) plane, (unsigned) context->Rbx);
      if (plane && getenv ("CHICAGO_GENERIC_IMPULSE_STEP") &&
          atoi (getenv ("CHICAGO_GENERIC_IMPULSE_STEP")) == adapt_trace_step)
        {
          memset ((void *) plane, 0, RAW_BYTES);
          if (destination)
            {
              memset ((void *) destination, 0, RAW_BYTES);
              ((uint16_t *) destination)[
                (HEIGHT / 2) * WIDTH + WIDTH / 2] = 0x2000;
            }
          printf ("generic filter impulse step=%d plane=%p\n",
                  adapt_trace_step, (void *) plane);
        }
      if (prefix && *prefix && plane)
        {
          snprintf (path, sizeof (path), "%s-generic-filter-before-%02d.bin",
                    prefix, adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite ((const void *) plane, 1, RAW_BYTES, output);
              fclose (output);
            }
          if (destination)
            {
              snprintf (path, sizeof (path),
                        "%s-generic-filter-destination-before-%02d.bin",
                        prefix, adapt_trace_step);
              output = fopen (path, "wb");
              if (output)
                {
                  fwrite ((const void *) destination, 1, RAW_BYTES, output);
                  fclose (output);
                }
            }
        }
      hit = adapt_generic_filter_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_generic_filter_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_filter_core_address || hit - 1 == adapt_filter_core_address)
    {
      const unsigned char *filter = (const unsigned char *) (uintptr_t)
        context->Rdi;
      const unsigned offsets[] = { 0x28, 0x50, 0x58, 0x60, 0x90, 0x98 };

      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      printf ("filter core step=%d object=%p\n", adapt_trace_step, filter);
      if (prefix && *prefix)
        {
          snprintf (path, sizeof (path), "%s-filter-object-%02d.bin", prefix,
                    adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite (filter, 1, 0xa0, output);
              fclose (output);
            }
          for (unsigned index = 0; index < sizeof (offsets) / sizeof (offsets[0]);
               index++)
            {
              const void *pointer = *(const void * const *) (filter + offsets[index]);
              MEMORY_BASIC_INFORMATION information;

              printf ("  +0x%x=%p\n", offsets[index], pointer);
              if (!pointer || !VirtualQuery (pointer, &information,
                                             sizeof (information)) ||
                  information.State != MEM_COMMIT ||
                  (const unsigned char *) pointer + 0x100 >
                    (const unsigned char *) information.BaseAddress +
                    information.RegionSize)
                continue;
              snprintf (path, sizeof (path), "%s-filter-p%02x-%02d.bin",
                        prefix, offsets[index], adapt_trace_step);
              output = fopen (path, "wb");
              if (output)
                {
                  fwrite (pointer, 1, 0x100, output);
                  fclose (output);
                }
              if (offsets[index] >= 0x90)
                {
                  const void *nested = *(const void * const *)
                    ((const unsigned char *) pointer + 0x20);

                  snprintf (path, sizeof (path),
                            "%s-filter-p%02x-data-%02d.bin", prefix,
                            offsets[index], adapt_trace_step);
                  output = nested ? fopen (path, "wb") : NULL;
                  if (output)
                    {
                      fwrite (nested, 1, 0x40, output);
                      fclose (output);
                    }
                }
            }
        }
      hit = adapt_filter_core_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_filter_core_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_filter_address || hit - 1 == adapt_filter_address)
    {
      if (getenv ("CHICAGO_FILTER_IMPULSE") &&
          atoi (getenv ("CHICAGO_FILTER_IMPULSE")) == adapt_trace_step)
        {
          uint16_t *target = *(uint16_t **) (uintptr_t) (context->Rsp + 0x60);

          memset (target, 0, RAW_BYTES);
          target[(HEIGHT / 2) * WIDTH + WIDTH / 2] = 0x2000;
          printf ("filter impulse step=%d target=%p\n", adapt_trace_step,
                  target);
        }
      VirtualProtect (adapt_filter_core_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *adapt_filter_core_address = 0xcc;
      VirtualProtect (adapt_filter_core_address, 1, protection, &protection);
      hit = adapt_filter_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_filter_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_average_return_address ||
      hit - 1 == adapt_average_return_address)
    {
      static const char *names[2] = { "average-after", "normalized-after" };
      const uintptr_t arguments[2] = {
        adapt_average_arguments[0], adapt_average_arguments[3]
      };

      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        for (unsigned index = 0; index < 2; index++)
          {
            snprintf (path, sizeof (path), "%s-average-%s-%02d.bin", prefix,
                      names[index], adapt_trace_step);
            output = fopen (path, "wb");
            if (output)
              {
                fwrite ((const void *) arguments[index], 1, RAW_BYTES,
                        output);
                fclose (output);
              }
          }
      hit = adapt_average_return_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_average_return_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_average_address || hit - 1 == adapt_average_address)
    {
      const uintptr_t arguments[4] = {
        context->Rcx, context->Rdx, context->R8, context->R9
      };
      static const char *names[3] = {
        "average-before", "candidate-u32", "normalized-before"
      };
      const unsigned count = *(const unsigned *) arguments[2];

      memcpy (adapt_average_arguments, arguments, sizeof (arguments));
      printf ("average step=%d average=%p candidate=%p count=%p(%u) "
              "normalized=%p config=%p defaults=%u/%u/%u\n",
              adapt_trace_step, (void *) arguments[0], (void *) arguments[1],
              (void *) arguments[2], count, (void *) arguments[3],
              *(void **) (uintptr_t) (context->Rsp + 0x28),
              *(const unsigned *) (chicago_base + 0x94c38),
              *(const unsigned *) (chicago_base + 0x94c3c),
              *(const unsigned *) (chicago_base + 0x94c40));
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        for (unsigned index = 0; index < 3; index++)
          {
            const unsigned argument_index = index == 2 ? 3 : index;
            const size_t size = index == 1 ? PIXELS * 4 : RAW_BYTES;

            snprintf (path, sizeof (path), "%s-average-%s-%02d.bin", prefix,
                      names[index], adapt_trace_step);
            output = fopen (path, "wb");
            if (output)
              {
                fwrite ((const void *) arguments[argument_index], 1, size,
                        output);
                fclose (output);
              }
          }
      hit = adapt_average_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_average_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_update_second_return_address ||
      hit - 1 == adapt_update_second_return_address)
    {
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        {
          snprintf (path, sizeof (path),
                    "%s-update2-target-after-%02d.bin", prefix,
                    adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite ((const void *) adapt_update_second_arguments[4], 1,
                      RAW_BYTES, output);
              fclose (output);
            }
        }
      hit = adapt_update_second_return_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_update_second_return_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_update_second_address ||
      hit - 1 == adapt_update_second_address)
    {
      static const char *names[7] = {
        "arg1", "object-plane", "arg3", "arg5", "arg8-target", "arg9-u32",
        "module-target"
      };
      const uintptr_t object = context->Rdx;
      const uintptr_t arguments[7] = {
        context->Rcx,
        object ? *(const uintptr_t *) (object + 0x18) : 0,
        context->R8,
        *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x20),
        *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x38),
        *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x40),
        (uintptr_t) (chicago_base + 0x116d70),
      };

      memcpy (adapt_update_second_arguments, arguments, sizeof (arguments));
      printf ("update2 step=%d arg1=%p object=%p plane=%p arg3=%p "
              "counter=%p(%u) arg5=%p dims=%u/%u target=%p arg9=%p "
              "module-target=%p\n",
              adapt_trace_step, (void *) arguments[0], (void *) object,
              (void *) arguments[1], (void *) arguments[2],
              (void *) (uintptr_t) context->R9,
              *(const unsigned *) (uintptr_t) context->R9,
              (void *) arguments[3],
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x28),
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x30),
              (void *) arguments[4], (void *) arguments[5],
              (void *) arguments[6]);
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        for (unsigned index = 0; index < 7; index++)
          if (arguments[index])
            {
              const size_t size = index == 5 ? PIXELS * 4 : RAW_BYTES;

              snprintf (path, sizeof (path), "%s-update2-%s-%02d.bin",
                        prefix, names[index], adapt_trace_step);
              output = fopen (path, "wb");
              if (output)
                {
                  fwrite ((const void *) arguments[index], 1, size, output);
                  fclose (output);
                }
            }
      hit = adapt_update_second_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_update_second_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_update_return_address ||
      hit - 1 == adapt_update_return_address)
    {
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        {
          snprintf (path, sizeof (path), "%s-update-target-after-%02d.bin",
                    prefix, adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite ((const void *) adapt_update_arguments[4], 1, RAW_BYTES,
                      output);
              fclose (output);
            }
        }
      hit = adapt_update_return_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_update_return_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_update_address || hit - 1 == adapt_update_address)
    {
      static const char *names[7] = {
        "arg1", "object-plane", "arg3", "arg5-target", "arg8", "arg9-u32",
        "module-target"
      };
      const uintptr_t object = context->Rdx;
      const uintptr_t arguments[7] = {
        context->Rcx,
        object ? *(const uintptr_t *) (object + 0x18) : 0,
        context->R8,
        *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x28),
        *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x40),
        *(const uintptr_t *) (uintptr_t) (context->Rsp + 0x48),
        (uintptr_t) (chicago_base + 0x116d70),
      };

      memcpy (adapt_update_arguments, arguments, sizeof (arguments));
      printf ("update step=%d arg1=%p object=%p plane=%p arg3=%p "
              "counter=%p(%u) arg5=%p dims=%u/%u arg8=%p arg9=%p "
              "module-target=%p\n",
              adapt_trace_step, (void *) arguments[0], (void *) object,
              (void *) arguments[1], (void *) arguments[2],
              (void *) (uintptr_t) context->R9,
              *(const unsigned *) (uintptr_t) context->R9,
              (void *) arguments[3],
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x30),
              *(const unsigned *) (uintptr_t) (context->Rsp + 0x38),
              (void *) arguments[4], (void *) arguments[5],
              (void *) arguments[6]);
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        for (unsigned index = 0; index < 7; index++)
          if (arguments[index])
            {
              const size_t size = index == 5 ? PIXELS * 4 : RAW_BYTES;

              snprintf (path, sizeof (path), "%s-update-%s-%02d.bin", prefix,
                        names[index], adapt_trace_step);
              output = fopen (path, "wb");
              if (output)
                {
                  fwrite ((const void *) arguments[index], 1, size, output);
                  fclose (output);
                }
            }
      hit = adapt_update_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_update_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_normalized_post_address ||
      hit - 1 == adapt_normalized_post_address)
    {
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        {
          const uintptr_t stack = (uintptr_t) context->Rsp;
          const uintptr_t sources[4] = {
            *(const uintptr_t *) (stack + 0x60),
            *(const uintptr_t *) (stack + 0x58),
            *(const uintptr_t *) (stack + 0xd8),
            *(const uintptr_t *) (stack + 0xc0) + 4,
          };
          const char *names[4] = {
            "denominator", "secondary", "numerator", "activity16"
          };

          snprintf (path, sizeof (path), "%s-stage-normalized-post-%02d.bin",
                    prefix, adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite (chicago_base + 0x116d70, 1, RAW_BYTES, output);
              fclose (output);
            }
          for (unsigned index = 0; index < 4; index++)
            {
              snprintf (path, sizeof (path), "%s-core-%s-%02d.bin", prefix,
                        names[index], adapt_trace_step);
              output = fopen (path, "wb");
              if (output)
                {
                  fwrite ((const void *) sources[index], 1, RAW_BYTES,
                          output);
                  fclose (output);
                }
            }
        }
      hit = adapt_normalized_post_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_normalized_post_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_normalized_address || hit - 1 == adapt_normalized_address)
    {
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        {
          snprintf (path, sizeof (path), "%s-stage-normalized-%02d.bin",
                    prefix, adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite (chicago_base + 0x116d70, 1, RAW_BYTES, output);
              fclose (output);
            }
        }
      hit = adapt_normalized_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_normalized_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_inputs_address || hit - 1 == adapt_inputs_address)
    {
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        {
          const uintptr_t sources[2] = { context->Rcx, context->Rdx };
          const char *names[2] = { "current", "base" };

          for (unsigned index = 0; index < 2; index++)
            {
              snprintf (path, sizeof (path), "%s-stage-%s-%02d.bin", prefix,
                        names[index], adapt_trace_step);
              output = fopen (path, "wb");
              if (output)
                {
                  fwrite ((const void *) sources[index], 1, RAW_BYTES, output);
                  fclose (output);
                }
            }
        }
      hit = adapt_inputs_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_inputs_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  for (unsigned stage = 0; stage < 3; stage++)
    if (hit == adapt_stage_addresses[stage] ||
        hit - 1 == adapt_stage_addresses[stage])
      {
        static const char *names[3] = { "source", "mask", "candidate" };
        const uintptr_t sources[3] = {
          context->Rbx + 0x13244,
          context->R9 + 0x10,
          context->Rbx + 0x1cb64,
        };
        const size_t sizes[3] = { RAW_BYTES, PIXELS, PIXELS };

        prefix = getenv ("CHICAGO_ADAPT_PREFIX");
        if (prefix && *prefix)
          {
            snprintf (path, sizeof (path), "%s-stage-%s-%02d.bin", prefix,
                      names[stage], adapt_trace_step);
            output = fopen (path, "wb");
            if (output)
              {
                fwrite ((const void *) sources[stage], 1, sizes[stage], output);
                fclose (output);
              }
          }
        hit = adapt_stage_addresses[stage];
        VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
        *hit = adapt_stage_originals[stage];
        VirtualProtect (hit, 1, protection, &protection);
        context->Rip = (DWORD64) (uintptr_t) hit;
        return EXCEPTION_CONTINUE_EXECUTION;
      }
  if (hit == adapt_core_address || hit - 1 == adapt_core_address)
    {
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      printf ("core step=%d rcx=%p rdx=%p r8=%p r9=%p config=%p\n",
              adapt_trace_step, (void *) (uintptr_t) context->Rcx,
              (void *) (uintptr_t) context->Rdx,
              (void *) (uintptr_t) context->R8,
              (void *) (uintptr_t) context->R9,
              *(void **) (uintptr_t) (context->Rsp + 0x38));
      if (prefix && *prefix)
        {
          const uintptr_t sources[2] = { context->Rcx, context->Rdx };
          const char *names[2] = { "core-rcx", "core-rdx" };

          for (unsigned index = 0; index < 2; index++)
            {
              snprintf (path, sizeof (path), "%s-%s-%02d.bin", prefix,
                        names[index], adapt_trace_step);
              output = fopen (path, "wb");
              if (output)
                {
                  fwrite ((const void *) sources[index], 1, RAW_BYTES, output);
                  fclose (output);
                }
            }
          snprintf (path, sizeof (path), "%s-core-state-%02d.bin", prefix,
                    adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite ((const void *) (uintptr_t) context->R8, 1, 0x23000,
                      output);
              fclose (output);
            }
          snprintf (path, sizeof (path), "%s-core-config-%02d.bin", prefix,
                    adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              fwrite (*(const void **) (uintptr_t) (context->Rsp + 0x38),
                      1, 0x40, output);
              fclose (output);
            }
        }
      hit = adapt_core_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_core_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit == adapt_mask_address || hit - 1 == adapt_mask_address)
    {
      prefix = getenv ("CHICAGO_ADAPT_PREFIX");
      if (prefix && *prefix)
        {
          snprintf (path, sizeof (path), "%s-mask-%02d.bin", prefix,
                    adapt_trace_step);
          output = fopen (path, "wb");
          if (output)
            {
              const void *mask = (const void *) (uintptr_t)
                *(const uintptr_t *) (uintptr_t) (context->Rbx + 0x28);

              fwrite (mask, 1, PIXELS, output);
              fclose (output);
            }
        }
      hit = adapt_mask_address;
      VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
      *hit = adapt_mask_original;
      VirtualProtect (hit, 1, protection, &protection);
      context->Rip = (DWORD64) (uintptr_t) hit;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (hit != adapt_trace_address && hit - 1 != adapt_trace_address)
    return EXCEPTION_CONTINUE_SEARCH;
  hit = adapt_trace_address;
  prefix = getenv ("CHICAGO_ADAPT_PREFIX");
  printf ("adapt step=%d source=%p state16=%p generation=%u pixels=%u "
          "activity=%p(%u) counters=%p count=%p(%u) threshold=%u avg=%u\n",
          adapt_trace_step, (void *) (uintptr_t) context->Rcx,
          (void *) (uintptr_t) context->Rdx, (unsigned) context->R8,
          (unsigned) context->R9,
          *(void **) (uintptr_t) (context->Rsp + 0x28),
          **(const unsigned **) (uintptr_t) (context->Rsp + 0x28),
          *(void **) (uintptr_t) (context->Rsp + 0x30),
          *(void **) (uintptr_t) (context->Rsp + 0x38),
          **(const unsigned **) (uintptr_t) (context->Rsp + 0x38),
          *(const unsigned *) (chicago_base + 0x94c10),
          *(const unsigned *) (chicago_base + 0x94c14));
  if (prefix && *prefix)
    {
      snprintf (path, sizeof (path), "%s-source-%02d.bin", prefix,
                adapt_trace_step);
      output = fopen (path, "wb");
      if (output)
        {
          fwrite ((const void *) (uintptr_t) context->Rcx, 1, RAW_BYTES,
                  output);
          fclose (output);
        }
      snprintf (path, sizeof (path), "%s-activity-%02d.bin", prefix,
                adapt_trace_step);
      output = fopen (path, "wb");
      if (output)
        {
          fwrite (*(const void **) (uintptr_t) (context->Rsp + 0x28), 1,
                  PIXELS + 0x10, output);
          fclose (output);
        }
    }
  VirtualProtect (hit, 1, PAGE_EXECUTE_READWRITE, &protection);
  *hit = adapt_trace_original;
  VirtualProtect (hit, 1, protection, &protection);
  context->Rip = (DWORD64) (uintptr_t) hit;
  return EXCEPTION_CONTINUE_EXECUTION;
}

static void
arm_adapt_trace (int step)
{
  DWORD protection;

  if (!getenv ("CHICAGO_ADAPT_PREFIX"))
    return;
  if (!adapt_trace_handler_installed)
    {
      adapt_trace_address = chicago_base + 0x3afc0;
      adapt_trace_original = *adapt_trace_address;
      adapt_mask_address = chicago_base + 0x3920b;
      adapt_mask_original = *adapt_mask_address;
      adapt_core_address = chicago_base + 0x4b300;
      adapt_core_original = *adapt_core_address;
      adapt_stage_addresses[0] = chicago_base + 0x48f67;
      adapt_stage_addresses[1] = chicago_base + 0x48f9e;
      adapt_stage_addresses[2] = chicago_base + 0x48fa3;
      for (unsigned stage = 0; stage < 3; stage++)
        adapt_stage_originals[stage] = *adapt_stage_addresses[stage];
      adapt_inputs_address = chicago_base + 0x48f62;
      adapt_inputs_original = *adapt_inputs_address;
      adapt_normalized_address = chicago_base + 0x4b520;
      adapt_normalized_original = *adapt_normalized_address;
      adapt_normalized_post_address = chicago_base + 0x4b5e3;
      adapt_normalized_post_original = *adapt_normalized_post_address;
      adapt_update_address = chicago_base + 0x4b8c0;
      adapt_update_original = *adapt_update_address;
      adapt_update_return_address = chicago_base + 0x4a6e6;
      adapt_update_return_original = *adapt_update_return_address;
      adapt_update_second_address = chicago_base + 0x4a72d;
      adapt_update_second_original = *adapt_update_second_address;
      adapt_update_second_return_address = chicago_base + 0x4a732;
      adapt_update_second_return_original = *adapt_update_second_return_address;
      adapt_average_address = chicago_base + 0x49990;
      adapt_average_original = *adapt_average_address;
      adapt_average_return_address = chicago_base + 0x4af29;
      adapt_average_return_original = *adapt_average_return_address;
      adapt_filter_address = chicago_base + 0x49bca;
      adapt_filter_original = *adapt_filter_address;
      adapt_filter_core_address = chicago_base + 0x4e732;
      adapt_filter_core_original = *adapt_filter_core_address;
      adapt_generic_filter_address = chicago_base + 0x4ba6d;
      adapt_generic_filter_original = *adapt_generic_filter_address;
      adapt_generic_filter_return_address = chicago_base + 0x4ba95;
      adapt_generic_filter_return_original =
        *adapt_generic_filter_return_address;
      AddVectoredExceptionHandler (1, adapt_trace_handler);
      adapt_trace_handler_installed = 1;
    }
  adapt_trace_step = step;
  VirtualProtect (adapt_trace_address, 1, PAGE_EXECUTE_READWRITE, &protection);
  *adapt_trace_address = 0xcc;
  VirtualProtect (adapt_trace_address, 1, protection, &protection);
  VirtualProtect (adapt_mask_address, 1, PAGE_EXECUTE_READWRITE, &protection);
  *adapt_mask_address = 0xcc;
  VirtualProtect (adapt_mask_address, 1, protection, &protection);
  VirtualProtect (adapt_core_address, 1, PAGE_EXECUTE_READWRITE, &protection);
  *adapt_core_address = 0xcc;
  VirtualProtect (adapt_core_address, 1, protection, &protection);
  for (unsigned stage = 0; stage < 3; stage++)
    {
      VirtualProtect (adapt_stage_addresses[stage], 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *adapt_stage_addresses[stage] = 0xcc;
      VirtualProtect (adapt_stage_addresses[stage], 1, protection,
                      &protection);
    }
  VirtualProtect (adapt_inputs_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_inputs_address = 0xcc;
  VirtualProtect (adapt_inputs_address, 1, protection, &protection);
  VirtualProtect (adapt_normalized_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_normalized_address = 0xcc;
  VirtualProtect (adapt_normalized_address, 1, protection, &protection);
  VirtualProtect (adapt_normalized_post_address, 1,
                  PAGE_EXECUTE_READWRITE, &protection);
  *adapt_normalized_post_address = 0xcc;
  VirtualProtect (adapt_normalized_post_address, 1, protection, &protection);
  VirtualProtect (adapt_update_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_update_address = 0xcc;
  VirtualProtect (adapt_update_address, 1, protection, &protection);
  VirtualProtect (adapt_update_return_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_update_return_address = 0xcc;
  VirtualProtect (adapt_update_return_address, 1, protection, &protection);
  VirtualProtect (adapt_update_second_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_update_second_address = 0xcc;
  VirtualProtect (adapt_update_second_address, 1, protection, &protection);
  VirtualProtect (adapt_update_second_return_address, 1,
                  PAGE_EXECUTE_READWRITE, &protection);
  *adapt_update_second_return_address = 0xcc;
  VirtualProtect (adapt_update_second_return_address, 1, protection,
                  &protection);
  VirtualProtect (adapt_average_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_average_address = 0xcc;
  VirtualProtect (adapt_average_address, 1, protection, &protection);
  VirtualProtect (adapt_average_return_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_average_return_address = 0xcc;
  VirtualProtect (adapt_average_return_address, 1, protection, &protection);
  VirtualProtect (adapt_filter_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_filter_address = 0xcc;
  VirtualProtect (adapt_filter_address, 1, protection, &protection);
  VirtualProtect (adapt_generic_filter_address, 1, PAGE_EXECUTE_READWRITE,
                  &protection);
  *adapt_generic_filter_address = 0xcc;
  VirtualProtect (adapt_generic_filter_address, 1, protection, &protection);
  VirtualProtect (adapt_generic_filter_return_address, 1,
                  PAGE_EXECUTE_READWRITE, &protection);
  *adapt_generic_filter_return_address = 0xcc;
  VirtualProtect (adapt_generic_filter_return_address, 1, protection,
                  &protection);
}
static FILE *position_map_output;

static uint64_t
fnv1a64 (const unsigned char *data,
         size_t               size)
{
  uint64_t hash = UINT64_C (1469598103934665603);

  for (size_t index = 0; data && index < size; index++)
    {
      hash ^= data[index];
      hash *= UINT64_C (1099511628211);
    }
  return hash;
}

static void
fp_state (unsigned       *mxcsr,
          unsigned short *x87cw)
{
  __asm__ volatile ("stmxcsr %0" : "=m" (*mxcsr));
  __asm__ volatile ("fnstcw %0" : "=m" (*x87cw));
}

static void
dump_position_maps (const unsigned char *enrollment,
                    uint32_t             count,
                    uint32_t             sequence)
{
  const uint32_t header[3] = { 0x50414d50, sequence, count }; /* PMAP */

  if (!position_map_output || !enrollment)
    return;
  fwrite (header, sizeof (header), 1, position_map_output);
  for (uint32_t index = 0; index < count; index++)
    {
      const unsigned char *subtemplate =
        *(const unsigned char * const *) (enrollment + 0x30 + index * 8);
      const unsigned char *object = subtemplate ?
        *(const unsigned char * const *) (subtemplate + 0x130) : NULL;
      const uint32_t bytes = object ? *(const uint32_t *) (object + 0x0c) : 0;
      const uint32_t item[2] = { index, bytes };

      fwrite (item, sizeof (item), 1, position_map_output);
      if (object)
        {
          fwrite (object, 1, 32, position_map_output);
          if (bytes && *(const void * const *) (object + 0x18))
            fwrite (*(const void * const *) (object + 0x18), 1, bytes,
                    position_map_output);
        }
    }
  fflush (position_map_output);
}

static LONG CALLBACK
relation_tap_handler (EXCEPTION_POINTERS *exception)
{
  CONTEXT *registers = exception->ContextRecord;

  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == overlap_tap_return_address)
    {
      DWORD protection;

      overlap_tap_record.result = (int32_t) registers->Rax;
      if (overlap_tap_scratch)
        memcpy (overlap_tap_record.scratch, overlap_tap_scratch,
                sizeof (overlap_tap_record.scratch));
      fwrite (&overlap_tap_record, sizeof (overlap_tap_record), 1,
              overlap_tap_output);
      fflush (overlap_tap_output);
      VirtualProtect (overlap_tap_return_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *overlap_tap_return_address = overlap_tap_return_original;
      FlushInstructionCache (GetCurrentProcess (), overlap_tap_return_address,
                             1);
      registers->Rip = (DWORD64) (uintptr_t) overlap_tap_return_address;
      overlap_tap_return_address = NULL;
      overlap_tap_scratch = NULL;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == overlap_tap_address)
    {
      const int32_t *transform =
        *(const int32_t **) (uintptr_t) (registers->Rsp + 0x28);
      DWORD protection;

      memset (&overlap_tap_record, 0, sizeof (overlap_tap_record));
      overlap_tap_record.relation_sequence = relation_tap_sequence - 1;
      overlap_tap_record.call_index = overlap_tap_call_index++;
      overlap_tap_record.dimensions[0] = (int32_t) registers->Rcx;
      overlap_tap_record.dimensions[1] = (int32_t) registers->Rdx;
      overlap_tap_record.dimensions[2] = (int32_t) registers->R8;
      overlap_tap_record.dimensions[3] = (int32_t) registers->R9;
      if (transform)
        memcpy (overlap_tap_record.transform, transform,
                sizeof (overlap_tap_record.transform));
      overlap_tap_scratch =
        *(int32_t **) (uintptr_t) (registers->Rsp + 0x30);
      overlap_tap_return_address =
        *(unsigned char **) (uintptr_t) registers->Rsp;
      overlap_tap_record.return_rva =
        (uint32_t) (overlap_tap_return_address - chicago_base);
      VirtualProtect (overlap_tap_return_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      overlap_tap_return_original = *overlap_tap_return_address;
      *overlap_tap_return_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), overlap_tap_return_address,
                             1);
      VirtualProtect (overlap_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *overlap_tap_address = overlap_tap_original;
      FlushInstructionCache (GetCurrentProcess (), overlap_tap_address, 1);
      registers->Rip = (DWORD64) (uintptr_t) overlap_tap_address;
      registers->EFlags |= 0x100;
      overlap_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == relation_tap_return_address)
    {
      const uint32_t result[4] = {
        0x52544552, /* RETR */
        relation_tap_input ? *(const uint32_t *) (relation_tap_input + 28) : 0,
        relation_tap_input ? *(const uint32_t *) (relation_tap_input + 32) : 0,
        relation_tap_input ? *(const uint32_t *) (relation_tap_input + 36) : 0,
      };
      DWORD protection;

      if (relation_tap_enrollment)
        {
          const uint32_t count =
            *(const uint32_t *) (relation_tap_enrollment + 0x24);
          const uint32_t transforms =
            *(const uint32_t *) (relation_tap_enrollment + 0x2c);

          for (uint32_t index = 0; index < count; index++)
            {
              const unsigned char *item =
                *(const unsigned char * const *)
                  (relation_tap_enrollment + 0x30 + index * 8);

              printf ("  relation-return sub[%u] group=%u state104=%u\n",
                      index,
                      item ? *(const uint32_t *) (item + 0x100) : 0,
                      item ? *(const uint32_t *) (item + 0x104) : 0);
            }
          for (uint32_t index = 0; index < transforms; index++)
            {
              const int32_t *relation =
                (const int32_t *) (relation_tap_enrollment + 0x1c0 +
                                    index * 0x1c);

              printf ("  relation-return relation[%u]=%d,%d,%d,%d,%d,%d,%d\n",
                      index, relation[0], relation[1], relation[2],
                      relation[3], relation[4], relation[5], relation[6]);
            }
        }
      fwrite (result, sizeof (result), 1, relation_tap_output);
      fflush (relation_tap_output);
      VirtualProtect (relation_tap_return_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *relation_tap_return_address = relation_tap_return_original;
      FlushInstructionCache (GetCurrentProcess (), relation_tap_return_address,
                             1);
      registers->Rip = (DWORD64) (uintptr_t) relation_tap_return_address;
      relation_tap_return_address = NULL;
      relation_tap_input = NULL;
      relation_tap_enrollment = NULL;
      if (overlap_tap_address)
        {
          VirtualProtect (overlap_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          *overlap_tap_address = overlap_tap_original;
          FlushInstructionCache (GetCurrentProcess (), overlap_tap_address, 1);
        }
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == group_tap_address)
    {
      const int32_t *transform =
        (const int32_t *) (uintptr_t) registers->R8;
      DWORD protection;

      printf ("  position-group index=%u transform=%d,%d,%d,%d,%d,%d\n",
              (uint32_t) registers->R9,
              transform ? transform[0] : 0,
              transform ? transform[1] : 0,
              transform ? transform[2] : 0,
              transform ? transform[3] : 0,
              transform ? transform[4] : 0,
              transform ? transform[5] : 0);
      printf ("  position-group anchor=%d flag=%d mode=%d\n",
              *(const int32_t *) ((uintptr_t) registers->Rcx + 0x87e0),
              *(const int32_t *) ((uintptr_t) registers->Rcx + 0x14),
              *(const int32_t *) ((uintptr_t) registers->Rcx + 0x87ec));
      VirtualProtect (group_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *group_tap_address = group_tap_original;
      FlushInstructionCache (GetCurrentProcess (), group_tap_address, 1);
      registers->Rip = (DWORD64) (uintptr_t) group_tap_address;
      registers->EFlags |= 0x100;
      group_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
      exception->ExceptionRecord->ExceptionAddress == relation_tap_address)
    {
      const unsigned char *enrollment =
        (const unsigned char *) (uintptr_t) registers->Rcx;
      const unsigned char *subtemplate =
        (const unsigned char *) (uintptr_t) registers->Rdx;
      const unsigned char *input =
        (const unsigned char *) (uintptr_t) registers->R8;
      const uint32_t relation_count = input ? *(const uint32_t *) (input + 4) : 0;
      const uint32_t *indices = input ? *(const uint32_t * const *) (input + 8) : NULL;
      const unsigned char *auxiliary =
        input ? *(const unsigned char * const *) (input + 16) : NULL;
      RelationTapHeader header = { 0, };
      DWORD protection;

      header.sequence = relation_tap_sequence++;
      header.relation_count = relation_count;
      header.input_index = input ? *(const uint32_t *) input : 0;
      header.input_threshold = input ? *(const uint32_t *) (input + 24) : 0;
      header.input_detail = input ? *(const uint32_t *) (input + 28) : 0;
      header.input_weight = input ? *(const uint32_t *) (input + 32) : 0;
      header.enrollment_count = enrollment ? *(const uint32_t *) (enrollment + 0x24) : 0;
      header.enrollment_capacity = enrollment ? *(const uint32_t *) (enrollment + 0x28) : 0;
      header.transform_count = enrollment ? *(const uint32_t *) (enrollment + 0x2c) : 0;

      for (uint32_t index = 0; enrollment && index < header.enrollment_count;
           index++)
        {
          const unsigned char *item =
            *(const unsigned char * const *) (enrollment + 0x30 + index * 8);

          printf ("  relation-tap[%u] sub[%u] records=%u group=%u state104=%u "
                  "active=%u q/c=%u/%u relation-base=%u\n",
                  header.sequence, index,
                  item ? *(const uint32_t *) (item + 0xf0) : 0,
                  item ? *(const uint32_t *) (item + 0x100) : 0,
                  item ? *(const uint32_t *) (item + 0x104) : 0,
                  item ? *(const uint32_t *) (item + 0x108) : 0,
                  item ? *(const uint32_t *) (item + 0x10c) : 0,
                  item ? *(const uint32_t *) (item + 0x110) : 0,
                  item ? *(const uint32_t *) (item + 0x11c) : 0);
        }

      dump_position_maps (enrollment, header.enrollment_count,
                          header.sequence);

      fwrite (&header, sizeof (header), 1, relation_tap_output);
      fwrite (enrollment, 1, 0x8c0, relation_tap_output);
      fwrite (subtemplate, 1, 0x160, relation_tap_output);
      if (relation_count && indices)
        fwrite (indices, sizeof (*indices), relation_count, relation_tap_output);
      if (relation_count && auxiliary)
        fwrite (auxiliary, 0x1c, relation_count, relation_tap_output);
      fflush (relation_tap_output);

      relation_tap_input = (unsigned char *) input;
      relation_tap_enrollment = (unsigned char *) enrollment;
      overlap_tap_call_index = 0;
      if (overlap_tap_address)
        {
          VirtualProtect (overlap_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          *overlap_tap_address = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), overlap_tap_address, 1);
        }
      relation_tap_return_address =
        *(unsigned char **) (uintptr_t) registers->Rsp;
      VirtualProtect (relation_tap_return_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      relation_tap_return_original = *relation_tap_return_address;
      *relation_tap_return_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), relation_tap_return_address,
                             1);

      VirtualProtect (relation_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *relation_tap_address = relation_tap_original;
      FlushInstructionCache (GetCurrentProcess (), relation_tap_address, 1);
      registers->Rip = (DWORD64) (uintptr_t) relation_tap_address;
      registers->EFlags |= 0x100;
      relation_tap_rearm = 1;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      (relation_tap_rearm || overlap_tap_rearm || group_tap_rearm))
    {
      DWORD protection;

      if (relation_tap_rearm)
        {
          VirtualProtect (relation_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          *relation_tap_address = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), relation_tap_address,
                                 1);
          relation_tap_rearm = 0;
        }
      if (overlap_tap_rearm)
        {
          VirtualProtect (overlap_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          *overlap_tap_address = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), overlap_tap_address, 1);
          overlap_tap_rearm = 0;
        }
      if (group_tap_rearm)
        {
          VirtualProtect (group_tap_address, 1, PAGE_EXECUTE_READWRITE,
                          &protection);
          *group_tap_address = 0xcc;
          FlushInstructionCache (GetCurrentProcess (), group_tap_address, 1);
          group_tap_rearm = 0;
        }
      registers->EFlags &= ~0x100;
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  return EXCEPTION_CONTINUE_SEARCH;
}

static void *
read_file (const char *path,
           size_t      bytes)
{
  FILE *file = fopen (path, "rb");
  void *data = calloc (1, bytes);

  if (!file || !data || fread (data, 1, bytes, file) != bytes)
    {
      if (file)
        fclose (file);
      free (data);
      return NULL;
    }
  fclose (file);
  return data;
}

static void
shift_enhanced (unsigned char *image,
                int            shift_x,
                int            shift_y)
{
  unsigned char shifted[PIXELS] = { 0, };

  for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < WIDTH; x++)
      {
        const int source_x = x - shift_x;
        const int source_y = y - shift_y;

        if (source_x >= 0 && source_x < WIDTH &&
            source_y >= 0 && source_y < HEIGHT)
          shifted[y * WIDTH + x] = image[source_y * WIDTH + source_x];
      }
  memcpy (image, shifted, sizeof (shifted));
}

static void
dump_algo_data_step (const char *phase,
                     int         step)
{
  const char *prefix = getenv ("CHICAGO_DATA_PREFIX");
  char path[MAX_PATH];
  FILE *output;

  if (!prefix || !*prefix || !chicago_base)
    return;
  snprintf (path, sizeof (path), "%s-%s-%02d.bin", prefix, phase, step);
  output = fopen (path, "wb");
  if (!output)
    return;
  /* The PE .data virtual section extends through zero-filled state up to
   * .pdata at RVA 0x149000; its on-disk raw size is only 0xb600. */
  fwrite (chicago_base + 0x8b000, 1, 0xbe000, output);
  fclose (output);

  snprintf (path, sizeof (path), "%s-%s-%02d-state16.bin",
            prefix, phase, step);
  output = fopen (path, "wb");
  if (output)
    {
      fwrite (chicago_base + 0xdb030, 1, RAW_BYTES, output);
      fclose (output);
    }
  snprintf (path, sizeof (path), "%s-%s-%02d-state8.bin",
            prefix, phase, step);
  output = fopen (path, "wb");
  if (output)
    {
      fwrite (chicago_base + 0xe4950, 1, PIXELS, output);
      fclose (output);
    }
}

int
main (int   argc,
      char *argv[])
{
  const char *calibration_path;
  HMODULE chicago;
  ppp_param_init_t ppp_param_init;
  preprocess_init_calidata_t preprocess_init_calidata;
  preprocess_get_calidata_len_t preprocess_get_calidata_len;
  preprocess_load_calidata_t preprocess_load_calidata;
  preprocessor_init_t preprocessor_init;
  preprocess_set_mode_t preprocess_set_mode;
  preprocessor_t preprocessor;
  enrol_start_t enrol_start;
  enrol_add_image_t enrol_add_image;
  enrol_get_template_t enrol_get_template;
  template_get_packed_size_t template_get_packed_size;
  template_pack_t template_pack;
  unsigned calibration_len;
  unsigned char *calibration;
  unsigned char *image_base;
  unsigned char config[HEADER_BYTES] = { 0, };
  unsigned char *initial_algo_state = NULL;
  void *context;
  FILE *output;
  uint32_t file_header[3];
  int steps;
  int warmup_count = 0;

  if (argc < 5 || ((argc - 3) & 1) != 0)
    {
      fprintf (stderr, "usage: %s <base.raw> <out.bin> <enhanced> <metadata> [...]\n",
               argv[0]);
      return 2;
    }
  steps = (argc - 3) / 2;
  if (getenv ("CHICAGO_WARMUP_COUNT"))
    {
      warmup_count = atoi (getenv ("CHICAGO_WARMUP_COUNT"));
      if (warmup_count < 0 || warmup_count >= steps)
        {
          fprintf (stderr, "invalid CHICAGO_WARMUP_COUNT=%d for %d steps\n",
                   warmup_count, steps);
          return 2;
        }
    }

  chicago = LoadLibraryA ("AlgoChicago.dll");
  chicago_base = (unsigned char *) chicago;
  if (chicago && getenv ("CHICAGO_RESOLUTION_TRACE"))
    {
      DWORD protection;

      resolution_tap_address = chicago_base + 0x53210;
      resolution_tap_original = *resolution_tap_address;
      resolution_class_tap_address = chicago_base + 0x13a89;
      resolution_class_tap_original = *resolution_class_tap_address;
      resolution_input_tap_address = chicago_base + 0x53df0;
      resolution_input_tap_original = *resolution_input_tap_address;
      resolution_code_tap_address = chicago_base + 0x54080;
      resolution_code_tap_original = *resolution_code_tap_address;
      resolution_context_tap_address = chicago_base + 0x13ef7;
      resolution_context_tap_original = *resolution_context_tap_address;
      resolution_scratch_tap_address = chicago_base + 0x44815;
      resolution_scratch_tap_original = *resolution_scratch_tap_address;
      resolution_labels_tap_address = chicago_base + 0x446eb;
      resolution_labels_tap_original = *resolution_labels_tap_address;
      resolution_stage_tap_addresses[0] = chicago_base + 0x38ae0;
      resolution_stage_tap_addresses[1] = chicago_base + 0x3ac50;
      resolution_stage_tap_addresses[2] = chicago_base + 0x38c4f;
      resolution_stage_tap_addresses[3] = chicago_base + 0x3adb9;
      resolution_stage_tap_addresses[4] = chicago_base + 0x3fbee;
      resolution_stage_tap_addresses[5] = chicago_base + 0x3ae1c;
      resolution_stage_tap_addresses[6] = chicago_base + 0x3fbb8;
      resolution_stage_tap_addresses[7] = chicago_base + 0x38cb6;
      resolution_stage_tap_addresses[8] = chicago_base + 0x38d07;
      resolution_stage_tap_addresses[9] = chicago_base + 0x3f4c0;
      resolution_stage_tap_addresses[10] = chicago_base + 0x3c860;
      resolution_stage_tap_addresses[11] = chicago_base + 0x39e70;
      resolution_stage_tap_addresses[12] = chicago_base + 0x4149c;
      for (int stage = 0; stage < 13; stage++)
        resolution_stage_tap_originals[stage] =
          *resolution_stage_tap_addresses[stage];
      AddVectoredExceptionHandler (1, resolution_tap_handler);
      VirtualProtect (resolution_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *resolution_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), resolution_tap_address, 1);
      VirtualProtect (resolution_class_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_class_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), resolution_class_tap_address,
                             1);
      VirtualProtect (resolution_input_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_input_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), resolution_input_tap_address,
                             1);
      VirtualProtect (resolution_code_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_code_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), resolution_code_tap_address,
                             1);
      VirtualProtect (resolution_context_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_context_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_context_tap_address, 1);
      VirtualProtect (resolution_scratch_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_scratch_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_scratch_tap_address, 1);
      VirtualProtect (resolution_labels_tap_address, 1,
                      PAGE_EXECUTE_READWRITE, &protection);
      *resolution_labels_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (),
                             resolution_labels_tap_address, 1);
      for (int stage = 0; stage < 13; stage++)
        {
          VirtualProtect (resolution_stage_tap_addresses[stage], 1,
                          PAGE_EXECUTE_READWRITE, &protection);
          *resolution_stage_tap_addresses[stage] = 0xcc;
          FlushInstructionCache (GetCurrentProcess (),
                                 resolution_stage_tap_addresses[stage], 1);
        }
    }
  if (chicago && getenv ("CHICAGO_RELATION_OUT"))
    {
      const uint32_t magic[3] = { 0x54414c52, 2, sizeof (RelationTapHeader) };
      DWORD protection;

      relation_tap_output = fopen (getenv ("CHICAGO_RELATION_OUT"), "wb");
      if (!relation_tap_output)
        return 1;
      fwrite (magic, sizeof (magic), 1, relation_tap_output);
      relation_tap_address = (unsigned char *) chicago + 0x19f30;
      group_tap_address = (unsigned char *) chicago + 0x31590;
      group_tap_original = *group_tap_address;
      if (getenv ("CHICAGO_OVERLAP_OUT"))
        {
          const uint32_t overlap_magic[3] = {
            0x504c564f, 1, sizeof (OverlapTapRecord), /* OVLP */
          };

          overlap_tap_output = fopen (getenv ("CHICAGO_OVERLAP_OUT"), "wb");
          if (!overlap_tap_output)
            return 1;
          fwrite (overlap_magic, sizeof (overlap_magic), 1,
                  overlap_tap_output);
          overlap_tap_address = (unsigned char *) chicago + 0x52030;
          overlap_tap_original = *overlap_tap_address;
        }
      if (getenv ("CHICAGO_POSITION_MAP_OUT"))
        {
          position_map_output =
            fopen (getenv ("CHICAGO_POSITION_MAP_OUT"), "wb");
          if (!position_map_output)
            return 1;
        }
      AddVectoredExceptionHandler (1, relation_tap_handler);
      VirtualProtect (relation_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      relation_tap_original = *relation_tap_address;
      *relation_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), relation_tap_address, 1);
      VirtualProtect (group_tap_address, 1, PAGE_EXECUTE_READWRITE,
                      &protection);
      *group_tap_address = 0xcc;
      FlushInstructionCache (GetCurrentProcess (), group_tap_address, 1);
    }
  ppp_param_init = EXPORT (chicago, ppp_param_init_t, "ppp_param_init");
  preprocess_init_calidata = EXPORT (chicago, preprocess_init_calidata_t,
                                     "preprocess_init_calidata");
  preprocess_get_calidata_len = EXPORT (chicago,
                                        preprocess_get_calidata_len_t,
                                        "preprocess_get_calidata_len");
  preprocess_load_calidata = EXPORT (chicago, preprocess_load_calidata_t,
                                     "preprocess_load_calidata");
  preprocessor_init = EXPORT (chicago, preprocessor_init_t,
                              "preprocessor_init");
  preprocess_set_mode = EXPORT (chicago, preprocess_set_mode_t,
                                "preprocess_set_mode");
  preprocessor = EXPORT (chicago, preprocessor_t, "preprocessor");
  enrol_start = EXPORT (chicago, enrol_start_t, "enrolStart");
  enrol_add_image = EXPORT (chicago, enrol_add_image_t, "enrolAddImage");
  enrol_get_template = EXPORT (chicago, enrol_get_template_t,
                               "enrolGetTemplate");
  template_get_packed_size = EXPORT (chicago, template_get_packed_size_t,
                                     "templateGetPackedSize");
  template_pack = EXPORT (chicago, template_pack_t, "templatePack");
  if (!chicago || !ppp_param_init || !preprocess_init_calidata ||
      !preprocess_get_calidata_len || !preprocess_load_calidata ||
      !preprocessor_init || !preprocess_set_mode || !preprocessor ||
      !enrol_start || !enrol_add_image ||
      !enrol_get_template || !template_get_packed_size || !template_pack)
    {
      fprintf (stderr, "missing AlgoChicago export\n");
      return 1;
    }

  calibration_path = getenv ("CHICAGO_CAL");
  if (!calibration_path || !*calibration_path)
    calibration_path = "Z:\\mnt\\win3\\ProgramData\\Goodix\\goodix_calib.dat";
  ppp_param_init (getenv ("PPP") ? atoi (getenv ("PPP")) : 12);
  preprocess_init_calidata ();
  calibration_len = preprocess_get_calidata_len ();
  calibration = read_file (calibration_path, 16 + calibration_len);
  image_base = read_file (argv[1], RAW_BYTES);
  if (!calibration || !image_base ||
      preprocess_load_calidata (calibration + 16, calibration_len) != 0)
    {
      fprintf (stderr, "could not initialize calibration\n");
      return 1;
    }
  if (getenv ("CHICAGO_DOUBLE_INIT"))
    {
      ppp_param_init (getenv ("PPP") ? atoi (getenv ("PPP")) : 12);
      preprocess_init_calidata ();
      if (preprocess_load_calidata (calibration + 16, calibration_len) != 0)
        {
          fprintf (stderr, "second calibration initialization failed\n");
          return 1;
        }
    }
  PTR (config, 0x18, image_base);
  U32 (config, 0x24, WIDTH);
  U32 (config, 0x28, HEIGHT);
  context = getenv ("CHICAGO_ENGINE_STATE") ? enrol_start () : NULL;
  if (preprocessor_init (config) != 0)
    {
      fprintf (stderr, "could not initialize ImageBase\n");
      return 1;
    }
  preprocess_set_mode (1);

  if (!context)
    context = enrol_start ();
  printf ("enrol globals=%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
          *(int *) (chicago_base + 0x9c590),
          *(int *) (chicago_base + 0x9c594),
          *(int *) (chicago_base + 0x9c598),
          *(int *) (chicago_base + 0x9c59c),
          *(int *) (chicago_base + 0x9c5a0),
          *(int *) (chicago_base + 0x9c5a4),
          *(int *) (chicago_base + 0x9c5a8),
          *(int *) (chicago_base + 0x9c5ac),
          *(int *) (chicago_base + 0x9c5b0));
  output = fopen (argv[2], "wb");
  if (!context || !output)
    return 1;
  if (getenv ("CHICAGO_RESET_OFFSET"))
    {
      initial_algo_state = malloc (0xbe000);
      if (!initial_algo_state)
        return 1;
      memcpy (initial_algo_state, chicago_base + 0x8b000, 0xbe000);
    }
  if (getenv ("CHICAGO_REQUIRED"))
    {
      const unsigned required = (unsigned) atoi (getenv ("CHICAGO_REQUIRED"));

      if (required == 0 || required > 50)
        {
          fprintf (stderr, "invalid CHICAGO_REQUIRED=%u\n", required);
          return 2;
        }
      *(uint16_t *) ((unsigned char *) context + 0x08) = (uint16_t) required;
      printf ("overrode internal required samples to %u\n", required);
    }
  file_header[0] = 0x53524e45; /* ENRS */
  file_header[1] = (uint32_t) (steps - warmup_count);
  file_header[2] = sizeof (EnrollmentStep);
  fwrite (file_header, sizeof (file_header), 1, output);

  for (int step = 0; step < steps; step++)
    {
      unsigned char *enhanced = read_file (argv[3 + step * 2], PIXELS);
      unsigned char *metadata = read_file (argv[4 + step * 2], HEADER_BYTES);
      unsigned char image[HEADER_BYTES] = { 0, };
      unsigned char scratch1[0x8000] = { 0, };
      unsigned char scratch2[0x8000] = { 0, };
      unsigned char enrol_metadata[0x1000] = { 0, };
      unsigned char *engine_feature = NULL;
      unsigned char *engine_enhanced = NULL;
      unsigned char *engine_raw = NULL;
      unsigned char *engine_raw_copy = NULL;
      unsigned char engine_input[HEADER_BYTES] = { 0, };
      unsigned char engine_temporary_output[HEADER_BYTES] = { 0, };
      void *add_image = image;
      void *add_scratch1 = scratch1;
      void *add_scratch2 = scratch2;
      void *add_metadata = enrol_metadata;
      const int engine_state = getenv ("CHICAGO_ENGINE_STATE") != NULL;
      unsigned char *holder;
      unsigned char *template_handle;
      unsigned char *inner;
      unsigned char *subtemplate = NULL;
      EnrollmentStep state = { 0, };
      unsigned mxcsr;
      unsigned short x87cw;

      if (engine_state)
        {
          if (step > 0 && initial_algo_state)
            {
              const size_t offset = strtoul (
                getenv ("CHICAGO_RESET_OFFSET"), NULL, 0);
              const size_t size = getenv ("CHICAGO_RESET_SIZE") ? strtoul (
                getenv ("CHICAGO_RESET_SIZE"), NULL, 0) : 1;

              if (offset > 0xbe000 || size > 0xbe000 - offset)
                {
                  fprintf (stderr, "invalid Chicago reset range\n");
                  return 2;
                }
              memcpy (chicago_base + 0x8b000 + offset,
                      initial_algo_state + offset, size);
              printf ("reset AlgoChicago state offset=0x%zx size=0x%zx\n",
                      offset, size);
            }
          free (enhanced);
          free (metadata);
          enhanced = NULL;
          metadata = NULL;
          engine_raw = read_file (argv[3 + step * 2], RAW_BYTES);
          engine_feature = calloc (1, 0x4d08);
          engine_enhanced = calloc (1, PIXELS);
          engine_raw_copy = calloc (1, RAW_BYTES);
          if (!engine_raw || !engine_feature || !engine_enhanced ||
              !engine_raw_copy)
            return 1;
          memcpy (engine_raw_copy, engine_raw, RAW_BYTES);

          PTR (engine_input, 0x00, engine_raw);
          /* EngineAdapter preserves the sensor's transport geometry in the
           * runtime image header: 80 columns by 64 rows.  Chicago's feature
           * internals reinterpret selected planes as 64 by 80 later; putting
           * that internal geometry here changes the production collector. */
          U16 (engine_input, 0x08, HEIGHT);
          U16 (engine_input, 0x0a, WIDTH);
          U32 (engine_input, 0x14, RAW_BYTES);
          U16 (engine_input, 0x18, 1);
          PTR (engine_feature, 0x00, engine_enhanced);
          U16 (engine_feature, 0x08, HEIGHT);
          U16 (engine_feature, 0x0a, WIDTH);
          U32 (engine_feature, 0x14, PIXELS);
          U16 (engine_feature, 0x18, 1);
          U8 (engine_feature, 0x0e, 8);
          U8 (engine_feature, 0x0f, 1);
          PTR (engine_feature, 0x4cf8, engine_raw_copy);

          if (step > 0 && getenv ("CHICAGO_REINIT_EACH"))
            {
              ppp_param_init (getenv ("PPP") ? atoi (getenv ("PPP")) : 12);
              preprocess_init_calidata ();
              if (preprocess_load_calidata (calibration + 16,
                                             calibration_len) != 0 ||
                  preprocessor_init (config) != 0)
                return 1;
              preprocess_set_mode (1);
            }

          dump_algo_data_step ("pre", step);
          arm_adapt_trace (step);
          const int preprocess_result =
            preprocessor (engine_input, engine_temporary_output,
                          engine_feature + 0x40, engine_feature,
                          engine_feature + 0x30, 0, 0);
          dump_algo_data_step ("post", step);
          if (getenv ("CHICAGO_ENHANCED_PREFIX"))
            {
              char enhanced_path[MAX_PATH];
              FILE *enhanced_output;

              snprintf (enhanced_path, sizeof (enhanced_path), "%s-%02d.bin",
                        getenv ("CHICAGO_ENHANCED_PREFIX"), step);
              enhanced_output = fopen (enhanced_path, "wb");
              if (enhanced_output)
                {
                  fwrite (engine_enhanced, 1, PIXELS, enhanced_output);
                  fclose (enhanced_output);
                }
            }
          printf ("step=%d preprocessor=0x%x quality=%u coverage=%u "
                  "state30=%u state34=%u scratch=%016llx enhanced=%016llx\n",
                  step, preprocess_result, engine_feature[0x28],
                  engine_feature[0x29],
                  *(uint32_t *) (engine_feature + 0x30),
                  *(uint32_t *) (engine_feature + 0x34),
                  (unsigned long long)
                    fnv1a64 (engine_feature + 0x40, 0x4cb8),
                  (unsigned long long) fnv1a64 (engine_enhanced, PIXELS));
          if (preprocess_result != 0)
            {
              fprintf (stderr, "preprocessor failed for step %d\n", step);
              return 1;
            }
          if (step < warmup_count)
            {
              printf ("step=%d preprocessing-only warmup (%d/%d)\n",
                      step, step + 1, warmup_count);
              free (engine_feature);
              free (engine_enhanced);
              free (engine_raw);
              free (engine_raw_copy);
              continue;
            }
          add_image = engine_feature;
          add_scratch1 = engine_feature + 0x40;
          add_scratch2 = engine_raw_copy;
          add_metadata = engine_feature + 0x30;
        }
      else
        {
          if (!enhanced || !metadata)
            return 1;
          if (step > 0 && (getenv ("CHICAGO_SHIFT_X") ||
                           getenv ("CHICAGO_SHIFT_Y")))
            shift_enhanced (enhanced,
                            getenv ("CHICAGO_SHIFT_X") ?
                              atoi (getenv ("CHICAGO_SHIFT_X")) : 0,
                            getenv ("CHICAGO_SHIFT_Y") ?
                              atoi (getenv ("CHICAGO_SHIFT_Y")) : 0);
          PTR (image, 0x00, enhanced);
          U16 (image, 0x08, WIDTH);
          U16 (image, 0x0a, HEIGHT);
          U8 (image, 0x0e, 8);
          U8 (image, 0x0f, 1);
          U32 (image, 0x14, PIXELS);
          U16 (image, 0x18, 1);
          U8 (image, 0x28, metadata[0x28]);
          U8 (image, 0x29, metadata[0x29]);
        }

      fp_state (&mxcsr, &x87cw);
      printf ("step=%d fp mxcsr=0x%08x x87cw=0x%04x\n",
              step, mxcsr, x87cw);
      if (step == 0 && getenv ("CHICAGO_DATA_OUT"))
        {
          FILE *data_output = fopen (getenv ("CHICAGO_DATA_OUT"), "wb");
          if (data_output)
            {
              fwrite (chicago_base + 0x8b000, 1, 0xb600, data_output);
              fclose (data_output);
            }
        }
      printf ("  add header dims=%u/%u bits=%u channels=%u frames=%u "
              "size=%u q/c=%u/%u state=%u/%u\n",
              *(uint16_t *) ((unsigned char *) add_image + 8),
              *(uint16_t *) ((unsigned char *) add_image + 0x0a),
              *((unsigned char *) add_image + 0x0e),
              *((unsigned char *) add_image + 0x0f),
              *(uint16_t *) ((unsigned char *) add_image + 0x18),
              *(uint32_t *) ((unsigned char *) add_image + 0x14),
              *((unsigned char *) add_image + 0x28),
              *((unsigned char *) add_image + 0x29),
              *(uint32_t *) ((unsigned char *) add_image + 0x30),
              *(uint32_t *) ((unsigned char *) add_image + 0x34));
      state.add_return = enrol_add_image (context, add_image, add_scratch1,
                                           add_scratch2,
                                           0,
                                           add_metadata);
      holder = *(unsigned char **) context;
      template_handle = holder ? *(unsigned char **) holder : NULL;
      inner = template_handle ? *(unsigned char **) template_handle : NULL;
      state.required = *(uint16_t *) ((unsigned char *) context + 0x08);
      state.accepted = *(uint16_t *) ((unsigned char *) context + 0x0a);
      state.progress = *(uint32_t *) ((unsigned char *) context + 0x0c);
      state.position_x = *(uint32_t *) ((unsigned char *) context + 0x10);
      state.position_y = *(uint32_t *) ((unsigned char *) context + 0x14);
      if (inner)
        {
          state.subtemplate_count = *(uint32_t *) (inner + 0x24);
          state.capacity = *(uint32_t *) (inner + 0x28);
          state.transform_count = *(uint32_t *) (inner + 0x2c);
          if (state.subtemplate_count != 0)
            subtemplate = *(unsigned char **) (inner + 0x30 +
                                                (state.subtemplate_count - 1) * 8);
          printf ("  inner type=%u dims=%u/%u state=%u/%u/%u/%u count=%u "
                  "capacity=%u transforms=%u\n",
                  *(uint32_t *) (inner + 0x08),
                  *(uint32_t *) (inner + 0x0c),
                  *(uint32_t *) (inner + 0x10),
                  *(uint32_t *) (inner + 0x14),
                  *(uint32_t *) (inner + 0x18),
                  *(uint32_t *) (inner + 0x1c),
                  *(uint32_t *) (inner + 0x20),
                  *(uint32_t *) (inner + 0x24),
                  *(uint32_t *) (inner + 0x28),
                  *(uint32_t *) (inner + 0x2c));
        }
      if (subtemplate)
        {
          state.record_count = *(uint32_t *) (subtemplate + 0xf0);
          state.active_count = *(uint32_t *) (subtemplate + 0x108);
          state.quality = *(uint32_t *) (subtemplate + 0x10c);
          state.coverage = *(uint32_t *) (subtemplate + 0x110);
          state.state_100 = *(uint32_t *) (subtemplate + 0x100);
          state.state_104 = *(uint32_t *) (subtemplate + 0x104);
        }
      fwrite (&state, sizeof (state), 1, output);
      printf ("step=%d ret=0x%x accepted=%u/%u progress=%u position=%u,%u "
              "subtemplates=%u transforms=%u records=%u active=%u q/c=%u/%u "
              "state=%u/%u\n",
              step, state.add_return, state.accepted, state.required,
              state.progress, state.position_x, state.position_y,
              state.subtemplate_count, state.transform_count,
              state.record_count, state.active_count,
              state.quality, state.coverage, state.state_100,
              state.state_104);
      free (enhanced);
      free (metadata);
      free (engine_feature);
      free (engine_enhanced);
      free (engine_raw);
      free (engine_raw_copy);
    }

  if (getenv ("CHICAGO_TEMPLATE_OUT"))
    {
      void *template_handle = NULL;
      int get_result = enrol_get_template (context, &template_handle);
      unsigned char *template_inner = template_handle ?
        *(unsigned char **) template_handle : NULL;
      int packed_size = template_handle ?
        template_get_packed_size (template_handle) : 0;
      unsigned char *packed = packed_size > 0 ?
        calloc (1, (size_t) packed_size) : NULL;
      int pack_result = packed ? template_pack (template_handle, packed) : -1;
      FILE *template_output = pack_result == 0 ?
        fopen (getenv ("CHICAGO_TEMPLATE_OUT"), "wb") : NULL;

      printf ("template get=0x%x handle=%p size=%d pack=0x%x\n",
              get_result, template_handle, packed_size, pack_result);
      if (template_inner)
        printf ("template inner=%p type=%u count=%u relations=%u group=%d\n",
                template_inner, *(unsigned *) (template_inner + 8),
                *(unsigned *) (template_inner + 0x24),
                *(unsigned *) (template_inner + 0x2c),
                *(int *) (template_inner + 0x87ec));
      if (template_output)
        {
          fwrite (packed, 1, (size_t) packed_size, template_output);
          fclose (template_output);
        }
      free (packed);
    }

  fclose (output);
  free (initial_algo_state);
  if (relation_tap_output)
    fclose (relation_tap_output);
  if (overlap_tap_output)
    fclose (overlap_tap_output);
  if (position_map_output)
    fclose (position_map_output);
  return 0;
}
