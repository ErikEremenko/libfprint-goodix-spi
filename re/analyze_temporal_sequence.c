/* Native sequential-preprocessing parity probe.  RE tooling only. */

#include <stdio.h>

#include "../lib/goodix/chicago/goodix-chicago-calibration.c"
#include "../lib/goodix/chicago/goodix-chicago-preprocess.c"

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
write_exact (const char *path,
             const void *data,
             gsize       size)
{
  FILE *file = fopen (path, "wb");
  gboolean ok = file && fwrite (data, 1, size, file) == size;

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
  g_autoptr(GoodixChicagoPreprocessor) preprocessor = NULL;
  guint16 base[GOODIX_CHICAGO_PIXELS];
  guint16 raw[2][GOODIX_CHICAGO_PIXELS];
  guint8 enhanced[GOODIX_CHICAGO_PIXELS];
  gsize calibration_size;

  if (argc != 6 ||
      !g_file_get_contents (argv[1], &calibration_data, &calibration_size,
                            &error) ||
      !read_exact (argv[2], base, sizeof (base)) ||
      !read_exact (argv[3], raw[0], sizeof (raw[0])) ||
      !read_exact (argv[4], raw[1], sizeof (raw[1])))
    return 2;
  calibration = goodix_chicago_calibration_load (
    argv[1], (const guint8 *) calibration_data, &error);
  if (!calibration)
    return 1;
  for (guint pixel = 0; pixel < GOODIX_CHICAGO_PIXELS; pixel++)
    {
      base[pixel] = GUINT16_FROM_LE (base[pixel]);
      raw[0][pixel] = GUINT16_FROM_LE (raw[0][pixel]);
      raw[1][pixel] = GUINT16_FROM_LE (raw[1][pixel]);
    }
  preprocessor = goodix_chicago_preprocessor_new (calibration, base, &error);
  if (!preprocessor)
    return 1;

  for (guint step = 0; step < 2; step++)
    {
      g_autofree gchar *enhanced_path = g_strdup_printf (
        "%s-enhanced-%02u.bin", argv[5], step);
      g_autofree gchar *normalized_path = g_strdup_printf (
        "%s-normalized-%02u.bin", argv[5], step);
      g_autofree gchar *source_path = g_strdup_printf (
        "%s-source-%02u.bin", argv[5], step);
      g_autofree gchar *mask_path = g_strdup_printf (
        "%s-mask-%02u.bin", argv[5], step);
      g_autofree gchar *candidate_path = g_strdup_printf (
        "%s-candidate-%02u.bin", argv[5], step);
      guint16 normalized[GOODIX_CHICAGO_PIXELS];
      guint16 current[GOODIX_CHICAGO_PIXELS];
      guint16 image_base[GOODIX_CHICAGO_PIXELS];
      guint16 source[GOODIX_CHICAGO_PIXELS];
      guint16 alternate_source[GOODIX_CHICAGO_PIXELS];
      guint8 mask[GOODIX_CHICAGO_PIXELS];
      guint8 candidate[GOODIX_CHICAGO_PIXELS];
      guint8 alternate[GOODIX_CHICAGO_PIXELS];
      GoodixChicagoPreprocessor snapshot = *preprocessor;
      const gboolean bootstrap_gain = snapshot.temporal_sample_count == 0;

      goodix_chicago_preprocessor_prepare_raw (&snapshot, raw[step], current);
      goodix_chicago_preprocessor_prepare_raw (&snapshot, base, image_base);
      build_source_plane_internal (&snapshot, current, image_base, FALSE,
                                   source);
      update_adaptive_multiplier (&snapshot, source);
      if (bootstrap_gain || snapshot.adaptive_sample_count > 5)
        build_source_plane_internal (&snapshot, current, image_base, TRUE,
                                     source);
      goodix_chicago_preprocessor_build_mask (&snapshot, current, image_base,
                                              mask);
      goodix_chicago_preprocessor_build_candidate (&snapshot, source, mask,
                                                   candidate);
      build_alternate_source (source, alternate_source);
      goodix_chicago_preprocessor_build_candidate (
        &snapshot, alternate_source, mask, alternate);

      {
        const gint primary_score = candidate_column_variation_score (
          candidate, mask);
        const gint alternate_score = candidate_column_variation_score (
          alternate, mask);
        const gint correlation = candidate_correlation_q8 (
          candidate, alternate, mask);

        printf ("step=%u primary=%d alternate=%d correlation=%d selected=%d\n",
                step, primary_score, alternate_score, correlation,
                goodix_chicago_preprocessor_select_alternate_mode24 (
                  primary_score, alternate_score, correlation, 600));
      }

      goodix_chicago_preprocessor_build_enhanced (
        preprocessor, raw[step], enhanced);
      for (guint pixel = 0; pixel < GOODIX_CHICAGO_PIXELS; pixel++)
        normalized[pixel] = GUINT16_TO_LE (
          preprocessor->normalized_gain[pixel]);
      if (!write_exact (enhanced_path, enhanced, sizeof (enhanced)) ||
          !write_exact (normalized_path, normalized, sizeof (normalized)) ||
          !write_exact (source_path, source, sizeof (source)) ||
          !write_exact (mask_path, mask, sizeof (mask)) ||
          !write_exact (candidate_path, candidate, sizeof (candidate)))
        return 1;
    }
  return 0;
}
