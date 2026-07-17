// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

/* Build a native two-capture Chicago enrollment for DLL parity comparison. */

#include <stdio.h>

#include "../drivers/gdix51c0/gdix51c0-chicago-calibration.c"
#include "../drivers/gdix51c0/gdix51c0-chicago-preprocess.c"
#define reflect_101 feature_reflect_101
#include "../drivers/gdix51c0/gdix51c0-chicago-feature.c"
#undef reflect_101
#include "../drivers/gdix51c0/gdix51c0-chicago-enrollment.c"

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

int
main (int   argc,
      char *argv[])
{
  g_autofree gchar *calibration_data = NULL;
  g_autoptr(GBytes) calibration = NULL;
  g_autoptr(GBytes) packed = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(Gdix51c0ChicagoPreprocessor) preprocessor = NULL;
  g_autoptr(Gdix51c0ChicagoEnrollment) enrollment = NULL;
  guint16 base[GDIX51C0_CHICAGO_PIXELS];
  guint16 raw[2][GDIX51C0_CHICAGO_PIXELS];
  gsize calibration_size;

  if (argc != 6 ||
      !g_file_get_contents (argv[1], &calibration_data, &calibration_size,
                            &error) ||
      !read_exact (argv[2], base, sizeof (base)) ||
      !read_exact (argv[3], raw[0], sizeof (raw[0])) ||
      !read_exact (argv[4], raw[1], sizeof (raw[1])))
    return 2;
  calibration = gdix51c0_chicago_calibration_load (
    argv[1], (const guint8 *) calibration_data, &error);
  if (!calibration)
    return 1;
  for (guint pixel = 0; pixel < GDIX51C0_CHICAGO_PIXELS; pixel++)
    {
      base[pixel] = GUINT16_FROM_LE (base[pixel]);
      raw[0][pixel] = GUINT16_FROM_LE (raw[0][pixel]);
      raw[1][pixel] = GUINT16_FROM_LE (raw[1][pixel]);
    }
  preprocessor = gdix51c0_chicago_preprocessor_new (calibration, base, &error);
  enrollment = gdix51c0_chicago_enrollment_new ();
  if (!preprocessor || !enrollment)
    return 1;

  for (guint step = 0; step < 2; step++)
    {
      guint8 enhanced[GDIX51C0_CHICAGO_PIXELS];
      Gdix51c0ChicagoFeatureRecord records[GDIX51C0_CHICAGO_FEATURE_RECORD_LIMIT];
      Gdix51c0ChicagoMetricData metric_data;
      Gdix51c0ChicagoEnrollmentResult result;
      guint active_count = 0;
      guint record_count;
      guint8 quality;
      guint8 coverage;
      gboolean ok;

      gdix51c0_chicago_preprocessor_build_enhanced (
        preprocessor, raw[step], enhanced);
      gdix51c0_chicago_preprocessor_finalize_metrics (
        gdix51c0_chicago_preprocessor_compute_base_quality_from_enhanced (
          enhanced),
        gdix51c0_chicago_preprocessor_compute_coverage (enhanced),
        &quality, &coverage);
      record_count = gdix51c0_chicago_feature_extract_subtemplate (
        enhanced, records, G_N_ELEMENTS (records), &active_count);
      gdix51c0_chicago_enrollment_build_metric_data (enhanced, &metric_data);
      ok = step == 0 ? gdix51c0_chicago_enrollment_insert_first (
        enrollment, records, record_count, active_count, quality, coverage,
        &metric_data, &result, &error) :
        gdix51c0_chicago_enrollment_insert_second (
          enrollment, records, record_count, active_count, quality, coverage,
          &metric_data, &result, &error);
      printf ("step=%u records=%u active=%u q/c=%u/%u position=%u/%u "
              "detail=0x%08x\n",
              step, record_count, active_count, quality, coverage,
              result.position_x, result.position_y,
              result.packed_position_detail);
      if (!ok)
        return 1;
      if (step == 1)
        {
          Gdix51c0ChicagoRelation relation;

          if (gdix51c0_chicago_enrollment_get_relation (
                enrollment, 1, &relation))
            printf ("relation inliers=%d transform=%d,%d,%d,%d,%d,%d\n",
                    relation.inlier_count, relation.transform[0],
                    relation.transform[1], relation.transform[2],
                    relation.transform[3], relation.transform[4],
                    relation.transform[5]);
          for (guint index = 0; index < 2; index++)
            {
              Gdix51c0ChicagoSubtemplateView view;
              guint ones = 0;

              gdix51c0_chicago_enrollment_get_subtemplate (
                enrollment, index, &view);
              for (guint byte = 0; byte < sizeof (view.metric_data->position_map);
                   byte++)
                ones += __builtin_popcount (
                  view.metric_data->position_map[byte]);
              printf ("subtemplate=%u group=%u position-ones=%u\n", index,
                      view.group_state, ones);
            }
        }
    }

  packed = gdix51c0_chicago_enrollment_pack (enrollment, &error);
  if (!packed)
    return 1;
  {
    const guint8 *data;
    gsize size;
    FILE *output;

    data = g_bytes_get_data (packed, &size);
    output = fopen (argv[5], "wb");
    if (!output || fwrite (data, 1, size, output) != size)
      return 1;
    fclose (output);
    printf ("packed=%zu\n", size);
  }
  return 0;
}
