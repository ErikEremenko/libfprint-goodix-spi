// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/*
 * Dump AlgoChicago's default calibration and the calibration produced after
 * preprocessor_init() consumes one 80x64 no-finger ImageBase.
 *
 * This is an offline reverse-engineering oracle only.  It is useful for
 * recovering the native first-run calibration path; the shipped driver must
 * not load this DLL.
 *
 * Build:
 *   x86_64-w64-mingw32-gcc -O2 chicago_calibration_seed.c \
 *     -o chicago_calibration_seed.exe
 * Run from the directory containing AlgoChicago.dll:
 *   WINEDEBUG=-all wine chicago_calibration_seed.exe base.raw before.bin init.bin
 * To exercise NeedUpdateImageBase on mature state, append an existing payload
 * or sensor-bound goodix_calib.dat as the fourth argument.
 */

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH       80u
#define HEIGHT      64u
#define PIXELS      (WIDTH * HEIGHT)
#define RAW_BYTES   (PIXELS * sizeof (unsigned short))
#define HEADER_SIZE 0x40u

#define EXPORT(module, type, name) \
  ((type) (void *) GetProcAddress ((module), (name)))
#define PTR(data, offset, value) \
  (*(void **) ((unsigned char *) (data) + (offset)) = (void *) (value))
#define U32(data, offset, value) \
  (*(unsigned *) ((unsigned char *) (data) + (offset)) = (unsigned) (value))

typedef int (*ppp_param_init_t) (int);
typedef int (*preprocess_init_calidata_t) (void);
typedef unsigned (*preprocess_get_calidata_len_t) (void);
typedef int (*preprocess_save_calidata_t) (void *, unsigned *);
typedef int (*preprocess_load_calidata_t) (void *, unsigned);
typedef int (*preprocessor_init_t) (void *);

static void *
read_exact (const char *path,
            size_t      size)
{
  FILE *file;
  void *data;

  file = fopen (path, "rb");
  if (!file)
    return NULL;
  data = calloc (1, size);
  if (!data || fread (data, 1, size, file) != size)
    {
      free (data);
      data = NULL;
    }
  fclose (file);
  return data;
}

static int
write_exact (const char *path,
             const void *data,
             size_t      size)
{
  FILE *file = fopen (path, "wb");
  int ok;

  if (!file)
    return 0;
  ok = fwrite (data, 1, size, file) == size;
  fclose (file);
  return ok;
}

static int
save_calibration (preprocess_save_calidata_t save,
                  const char                *path,
                  unsigned                   expected_length)
{
  unsigned char *payload = calloc (1, expected_length);
  unsigned length = expected_length;
  int ret;

  if (!payload)
    return 0;
  ret = save (payload, &length);
  printf ("preprocess_save_calidata(%s) ret=0x%x len=0x%x\n",
          path, ret, length);
  if (ret || length != expected_length ||
      !write_exact (path, payload, length))
    {
      free (payload);
      return 0;
    }
  free (payload);
  return 1;
}

int
main (int   argc,
      char *argv[])
{
  HMODULE chicago;
  ppp_param_init_t ppp_param_init;
  preprocess_init_calidata_t preprocess_init_calidata;
  preprocess_get_calidata_len_t preprocess_get_calidata_len;
  preprocess_save_calidata_t preprocess_save_calidata;
  preprocess_load_calidata_t preprocess_load_calidata;
  preprocessor_init_t preprocessor_init;
  unsigned short *image_base;
  unsigned char *existing_calibration = NULL;
  unsigned char config[HEADER_SIZE] = { 0 };
  unsigned calibration_length;
  int ret;

  if (argc != 4 && argc != 5)
    {
      fprintf (stderr, "usage: %s <base.raw> <before.bin> <initialized.bin> [existing-calibration]\n",
               argv[0]);
      return 2;
    }

  chicago = LoadLibraryA ("AlgoChicago.dll");
  ppp_param_init = EXPORT (chicago, ppp_param_init_t, "ppp_param_init");
  preprocess_init_calidata = EXPORT (chicago, preprocess_init_calidata_t,
                                     "preprocess_init_calidata");
  preprocess_get_calidata_len = EXPORT (chicago,
                                        preprocess_get_calidata_len_t,
                                        "preprocess_get_calidata_len");
  preprocess_save_calidata = EXPORT (chicago, preprocess_save_calidata_t,
                                     "preprocess_save_calidata");
  preprocess_load_calidata = EXPORT (chicago, preprocess_load_calidata_t,
                                     "preprocess_load_calidata");
  preprocessor_init = EXPORT (chicago, preprocessor_init_t,
                              "preprocessor_init");
  if (!chicago || !ppp_param_init || !preprocess_init_calidata ||
      !preprocess_get_calidata_len || !preprocess_save_calidata ||
      !preprocess_load_calidata || !preprocessor_init)
    {
      fprintf (stderr, "missing AlgoChicago export\n");
      return 1;
    }

  image_base = read_exact (argv[1], RAW_BYTES);
  if (!image_base)
    {
      fprintf (stderr, "could not read 0x%x-byte ImageBase %s\n",
               (unsigned) RAW_BYTES, argv[1]);
      return 1;
    }

  ret = ppp_param_init (12);
  printf ("ppp_param_init(12) ret=0x%x\n", ret);
  calibration_length = preprocess_get_calidata_len ();
  printf ("preprocess_get_calidata_len=0x%x\n", calibration_length);
  if (ret || calibration_length != 0x224b0)
    return 1;

  if (argc == 5)
    {
      FILE *file = fopen (argv[4], "rb");
      long file_size;
      unsigned payload_offset;

      if (!file || fseek (file, 0, SEEK_END) ||
          (file_size = ftell (file)) < 0 || fseek (file, 0, SEEK_SET))
        {
          fprintf (stderr, "could not inspect calibration %s\n", argv[4]);
          return 1;
        }
      payload_offset = file_size == (long) calibration_length + 16 ? 16 : 0;
      if (file_size != (long) (calibration_length + payload_offset))
        {
          fprintf (stderr, "unexpected calibration size 0x%lx\n", file_size);
          return 1;
        }
      existing_calibration = calloc (1, (size_t) file_size);
      if (!existing_calibration ||
          fread (existing_calibration, 1, (size_t) file_size, file) !=
          (size_t) file_size)
        return 1;
      fclose (file);
      ret = preprocess_load_calidata (existing_calibration + payload_offset,
                                      calibration_length);
      printf ("preprocess_load_calidata ret=0x%x\n", ret);
    }
  else
    {
      ret = preprocess_init_calidata ();
      printf ("preprocess_init_calidata ret=0x%x\n", ret);
    }

  if (ret ||
      !save_calibration (preprocess_save_calidata, argv[2],
                         calibration_length))
    return 1;

  PTR (config, 0x18, image_base);
  U32 (config, 0x24, WIDTH);
  U32 (config, 0x28, HEIGHT);
  ret = preprocessor_init (config);
  printf ("preprocessor_init ret=0x%x\n", ret);
  if (ret || !save_calibration (preprocess_save_calidata, argv[3],
                                calibration_length))
    return 1;

  free (image_base);
  free (existing_calibration);
  return 0;
}
