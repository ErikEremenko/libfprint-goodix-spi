// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

/* Test whether AlgoChicago's persistent 5,120-word plane is the gain plane
 * consumed by the recovered native preprocessor.  This is RE tooling, not a
 * driver component. */

#include <stdio.h>
#include <string.h>

#include "../drivers/gdix51c0/gdix51c0-chicago-calibration.c"
#include "../drivers/gdix51c0/gdix51c0-chicago-preprocess.c"

static gboolean
read_exact (const char *path,
            void       *data,
            gsize       size)
{
  FILE *file = fopen (path, "rb");
  gboolean ok = file && fread (data, 1, size, file) == size;

  if (file)
    fclose (file);
  return ok;
}

static gboolean
read_exact_offset (const char *path,
                   gsize       offset,
                   void       *data,
                   gsize       size)
{
  FILE *file = fopen (path, "rb");
  gboolean ok = file && fseek (file, offset, SEEK_SET) == 0 &&
                fread (data, 1, size, file) == size;

  if (file)
    fclose (file);
  return ok;
}

int
main (int   argc,
      char *argv[])
{
  g_autofree gchar *calibration_data = NULL;
  g_autoptr(GBytes) calibration = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(Gdix51c0ChicagoPreprocessor) preprocessor = NULL;
  guint16 base[GDIX51C0_CHICAGO_PIXELS];
  guint16 raw[GDIX51C0_CHICAGO_PIXELS];
  guint16 state[GDIX51C0_CHICAGO_PIXELS];
  guint16 current[GDIX51C0_CHICAGO_PIXELS];
  guint16 prepared_base[GDIX51C0_CHICAGO_PIXELS];
  guint16 source[GDIX51C0_CHICAGO_PIXELS];
  guint8 mask[GDIX51C0_CHICAGO_PIXELS];
  guint8 enhanced[GDIX51C0_CHICAGO_PIXELS];
  gsize calibration_size;
  FILE *output;

  if (argc != 7)
    return 2;
  if (!g_file_get_contents (argv[1], &calibration_data, &calibration_size,
                            &error) ||
      !read_exact (argv[2], base, sizeof (base)) ||
      !read_exact (argv[3], raw, sizeof (raw)) ||
      !((g_str_equal (argv[6], "gainraw") ||
         g_str_equal (argv[6], "gainfixed") ||
         g_str_has_prefix (argv[6], "delta")) ?
        read_exact_offset (argv[4], 4, state, sizeof (state)) :
        read_exact (argv[4], state, sizeof (state))))
    return 1;
  calibration = gdix51c0_chicago_calibration_load (
    argv[1], (const guint8 *) calibration_data, &error);
  if (!calibration)
    return 1;
  for (guint i = 0; i < GDIX51C0_CHICAGO_PIXELS; i++)
    {
      base[i] = GUINT16_FROM_LE (base[i]);
      raw[i] = GUINT16_FROM_LE (raw[i]);
      state[i] = GUINT16_FROM_LE (state[i]);
    }
  preprocessor = gdix51c0_chicago_preprocessor_new (calibration, base, &error);
  if (!preprocessor)
    return 1;
  for (guint i = 0; i < GDIX51C0_CHICAGO_PIXELS; i++)
    {
      if (g_str_equal (argv[6], "state"))
        preprocessor->normalized_gain[i] = state[i];
      else if (g_str_equal (argv[6], "mul"))
        preprocessor->normalized_gain[i] =
          ((guint32) preprocessor->normalized_gain[i] * state[i] + 0x1000) >> 13;
      else if (g_str_equal (argv[6], "div"))
        preprocessor->normalized_gain[i] =
          ((guint32) preprocessor->normalized_gain[i] * 0x2000 + state[i] / 2) /
          state[i];
      else if (g_str_equal (argv[6], "gainraw"))
        preprocessor->gain[i] = state[i];
      else if (g_str_equal (argv[6], "gainfixed"))
        preprocessor->gain[i] = state[i];
      else if (g_str_has_prefix (argv[6], "delta"))
        {
          guint16 original_gain;
          guint16 offset;
          const gint factor = atoi (argv[6] + strlen ("delta"));

          gdix51c0_chicago_calibration_get_corrections (
            calibration, i, &original_gain, &offset, NULL);
          preprocessor->normalized_gain[i] +=
            factor * ((gint) state[i] - original_gain);
        }
      else
        return 2;
    }
  if (g_str_equal (argv[6], "gainraw"))
    {
      preprocessor->normalized_gain_valid = FALSE;
      normalize_gain_map (preprocessor);
    }
  else if (g_str_equal (argv[6], "gainfixed"))
    {
      guint64 original_sum = 0;
      guint32 original_mean;

      for (guint i = 0; i < GDIX51C0_CHICAGO_PIXELS; i++)
        {
          guint16 gain;
          guint16 offset;

          gdix51c0_chicago_calibration_get_corrections (
            calibration, i, &gain, &offset, NULL);
          original_sum += gain;
        }
      original_mean = (original_sum + GDIX51C0_CHICAGO_PIXELS / 2) /
                      GDIX51C0_CHICAGO_PIXELS;
      for (guint i = 0; i < GDIX51C0_CHICAGO_PIXELS; i++)
        preprocessor->normalized_gain[i] =
          ((guint32) preprocessor->gain[i] * 0x2000 + original_mean / 2) /
          original_mean;
      preprocessor->normalized_gain_valid = TRUE;
    }
  preprocessor->normalized_gain_valid = TRUE;
  {
    g_autofree gchar *normalized_path = g_strconcat (
      argv[5], ".normalized", NULL);

    output = fopen (normalized_path, "wb");
    if (!output)
      return 1;
    for (guint i = 0; i < GDIX51C0_CHICAGO_PIXELS; i++)
      {
        const guint16 value = GUINT16_TO_LE (
          preprocessor->normalized_gain[i]);
        fwrite (&value, 1, sizeof (value), output);
      }
    fclose (output);
  }
  gdix51c0_chicago_preprocessor_prepare_raw (preprocessor, raw, current);
  gdix51c0_chicago_preprocessor_prepare_raw (preprocessor, base,
                                              prepared_base);
  {
    g_autofree gchar *current_path = g_strconcat (
      argv[5], ".current", NULL);
    g_autofree gchar *base_path = g_strconcat (argv[5], ".base", NULL);

    output = fopen (current_path, "wb");
    if (!output)
      return 1;
    for (guint i = 0; i < GDIX51C0_CHICAGO_PIXELS; i++)
      {
        const guint16 value = GUINT16_TO_LE (current[i]);
        fwrite (&value, 1, sizeof (value), output);
      }
    fclose (output);
    output = fopen (base_path, "wb");
    if (!output)
      return 1;
    for (guint i = 0; i < GDIX51C0_CHICAGO_PIXELS; i++)
      {
        const guint16 value = GUINT16_TO_LE (prepared_base[i]);
        fwrite (&value, 1, sizeof (value), output);
      }
    fclose (output);
  }
  gdix51c0_chicago_preprocessor_build_source_plane (
    preprocessor, current, prepared_base, source);
  gdix51c0_chicago_preprocessor_build_mask (
    preprocessor, current, prepared_base, mask);
  {
    g_autofree gchar *source_path = g_strconcat (argv[5], ".source", NULL);
    g_autofree gchar *mask_path = g_strconcat (argv[5], ".mask", NULL);

    output = fopen (source_path, "wb");
    if (!output)
      return 1;
    for (guint i = 0; i < GDIX51C0_CHICAGO_PIXELS; i++)
      {
        const guint16 value = GUINT16_TO_LE (source[i]);
        fwrite (&value, 1, sizeof (value), output);
      }
    fclose (output);
    output = fopen (mask_path, "wb");
    if (!output)
      return 1;
    fwrite (mask, 1, sizeof (mask), output);
    fclose (output);
  }
  gdix51c0_chicago_preprocessor_build_enhanced (preprocessor, raw, enhanced);
  output = fopen (argv[5], "wb");
  if (!output)
    return 1;
  fwrite (enhanced, 1, sizeof (enhanced), output);
  fclose (output);
  return 0;
}
