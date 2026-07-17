/* Replay an ordered raw capture stream through the native live enrollment path. */

#include <stdio.h>

#include "../drivers/gdix51c0/gdix51c0-chicago-calibration.c"
#include "../drivers/gdix51c0/gdix51c0-chicago-preprocess.c"
#define reflect_101 feature_reflect_101
#include "../drivers/gdix51c0/gdix51c0-chicago-feature.c"
#undef reflect_101
#include "../drivers/gdix51c0/gdix51c0-chicago-enrollment.c"

#define MIN_QUALITY 25u
#define MIN_COVERAGE 65u

static guint64
fnv1a64 (const guint8 *data,
         gsize         size)
{
  guint64 hash = G_GUINT64_CONSTANT (1469598103934665603);

  for (gsize index = 0; index < size; index++)
    {
      hash ^= data[index];
      hash *= G_GUINT64_CONSTANT (1099511628211);
    }
  return hash;
}

static gboolean
read_raw16 (const char *path,
            guint16     pixels[GDIX51C0_CHICAGO_PIXELS])
{
  FILE *file = fopen (path, "rb");
  gboolean ok = file && fread (pixels, sizeof (*pixels),
                                GDIX51C0_CHICAGO_PIXELS, file) ==
                         GDIX51C0_CHICAGO_PIXELS;

  if (file)
    fclose (file);
  if (!ok)
    return FALSE;
  for (guint pixel = 0; pixel < GDIX51C0_CHICAGO_PIXELS; pixel++)
    pixels[pixel] = GUINT16_FROM_LE (pixels[pixel]);
  return TRUE;
}

static gboolean
dump_temporal_state (const Gdix51c0ChicagoPreprocessor *preprocessor,
                     guint                              step)
{
  const gchar *prefix = g_getenv ("NATIVE_TEMPORAL_PREFIX");
  g_autofree gchar *average_path = NULL;
  g_autofree gchar *normalized_path = NULL;
  guint16 average[GDIX51C0_CHICAGO_PIXELS];
  guint16 normalized[GDIX51C0_CHICAGO_PIXELS];

  if (!prefix || !*prefix)
    return TRUE;
  for (guint pixel = 0; pixel < GDIX51C0_CHICAGO_PIXELS; pixel++)
    {
      average[pixel] = GUINT16_TO_LE (
        preprocessor->temporal_gain_average[pixel]);
      normalized[pixel] = GUINT16_TO_LE (
        preprocessor->normalized_gain[pixel]);
    }
  average_path = g_strdup_printf ("%s-average-after-%02u.bin", prefix, step);
  normalized_path = g_strdup_printf (
    "%s-normalized-after-%02u.bin", prefix, step);
  return g_file_set_contents (average_path, (const gchar *) average,
                              sizeof (average), NULL) &&
         g_file_set_contents (normalized_path, (const gchar *) normalized,
                              sizeof (normalized), NULL);
}

static gboolean
apply_native_resolution_map (Gdix51c0ChicagoPreprocessor *preprocessor,
                             const guint16                raw[GDIX51C0_CHICAGO_PIXELS],
                             Gdix51c0ChicagoMetricData   *metric_data,
                             guint                         step,
                             GError                      **error)
{
  const gchar *directory = g_getenv ("NATIVE_RESOLUTION_LABEL_DIR");
  guint8 labels[GDIX51C0_CHICAGO_PIXELS];
  g_autofree gchar *basename = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *contents = NULL;
  gsize size = 0;
  gboolean auxiliary;
  guint code;
  guint peak_state;
  Gdix51c0ChicagoFeatureLiveAuxiliary live_auxiliary;

  if (g_getenv ("NATIVE_RESOLUTION_CONTEXT_TRACE"))
    {
      guint16 current[GDIX51C0_CHICAGO_PIXELS];
      guint16 image_base[GDIX51C0_CHICAGO_PIXELS];
      guint16 base[GDIX51C0_CHICAGO_PIXELS];
      guint16 secondary[GDIX51C0_CHICAGO_PIXELS];
      guint16 filtered[GDIX51C0_CHICAGO_PIXELS];
      guint8 input_mask[GDIX51C0_CHICAGO_PIXELS];
      Gdix51c0ChicagoResolutionSecondaryAnalysis analysis = { 0, };
      Gdix51c0ChicagoResolutionStatistics statistics = { 0, };
      gboolean use_exceptional;
      gboolean use_normal;
      gint branch_state;

      gdix51c0_chicago_preprocessor_prepare_raw (
        preprocessor, raw, current);
      gdix51c0_chicago_preprocessor_prepare_raw (
        preprocessor,
        gdix51c0_chicago_preprocessor_get_image_base (preprocessor),
        image_base);
      gdix51c0_chicago_preprocessor_build_resolution_base_plane (
        current, image_base, base);
      gdix51c0_chicago_preprocessor_build_resolution_secondary_plane (
        base, secondary);
      gdix51c0_chicago_preprocessor_build_resolution_input_mask (input_mask);
      gdix51c0_chicago_preprocessor_calculate_resolution_secondary_analysis (
        secondary, input_mask, &analysis);
      gdix51c0_chicago_preprocessor_build_resolution_filtered_gradient (
        secondary, input_mask, filtered);
      gdix51c0_chicago_preprocessor_calculate_resolution_primary_statistics (
        filtered, input_mask, &statistics);
      gdix51c0_chicago_preprocessor_select_resolution_branches (
        &analysis, &statistics, &use_exceptional, &use_normal, &branch_state);
      printf ("  resolution-context-native peak=%d branches=%u,%u state=%d\n",
              analysis.peak_state, use_exceptional, use_normal, branch_state);
    }

  gdix51c0_chicago_preprocessor_build_resolution_map_full (
    preprocessor, raw, labels, &peak_state);
  if (directory && *directory)
    {
      basename = g_strdup_printf ("resolution-labels-%02u.bin", step);
      path = g_build_filename (directory, basename, NULL);
      if (!g_file_get_contents (path, &contents, &size, error))
        return FALSE;
      if (size != GDIX51C0_CHICAGO_PIXELS)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       "%s has %" G_GSIZE_FORMAT " bytes, expected %u",
                       path, size, GDIX51C0_CHICAGO_PIXELS);
          return FALSE;
        }
      if (memcmp (labels, contents, sizeof (labels)) != 0)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       "native resolution labels differ from %s", path);
          return FALSE;
        }
    }

  gdix51c0_chicago_preprocessor_classify_resolution_labels (
    0x18, labels, GDIX51C0_CHICAGO_PIXELS, GDIX51C0_CHICAGO_PIXELS,
    &code, &auxiliary);
  metric_data->packed_resolution =
    gdix51c0_chicago_preprocessor_pack_resolution_code (code);
  gdix51c0_chicago_feature_build_live_auxiliary (
    peak_state, metric_data->packed_resolution, &live_auxiliary);
  printf ("  resolution-native code=%u packed=0x%x auxiliary=%u\n",
          code, metric_data->packed_resolution, auxiliary);
  printf ("  live-auxiliary-native values=%u,%u,%u,%u,%u,%u\n",
          live_auxiliary.values[0], live_auxiliary.values[1],
          live_auxiliary.values[2], live_auxiliary.values[3],
          live_auxiliary.values[4], live_auxiliary.values[5]);
  return TRUE;
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
  Gdix51c0ChicagoEngineEnrollmentPolicy policy;
  g_autofree Gdix51c0ChicagoFeatureRecord *deferred_records = NULL;
  Gdix51c0ChicagoMetricData deferred_metric_data;
  guint deferred_record_count = 0;
  guint deferred_active_count = 0;
  guint deferred_quality = 0;
  guint deferred_coverage = 0;
  guint16 base[GDIX51C0_CHICAGO_PIXELS];
  gsize calibration_size;
  guint warmup_count;

  if (argc < 6)
    {
      fprintf (stderr, "usage: %s <calibration> <base.raw> <packed-out> "
                       "<warmup-count> <capture.raw> [...]\n", argv[0]);
      return 2;
    }
  warmup_count = (guint) g_ascii_strtoull (argv[4], NULL, 0);
  if (warmup_count > (guint) argc - 5)
    {
      fprintf (stderr, "warmup count exceeds capture count\n");
      return 2;
    }
  if (!g_file_get_contents (argv[1], &calibration_data, &calibration_size,
                            &error) ||
      !read_raw16 (argv[2], base))
    {
      fprintf (stderr, "could not read calibration/base: %s\n",
               error ? error->message : "invalid raw base");
      return 1;
    }
  calibration = gdix51c0_chicago_calibration_load (
    argv[1], (const guint8 *) calibration_data, &error);
  if (!calibration)
    {
      fprintf (stderr, "could not load calibration: %s\n", error->message);
      return 1;
    }
  preprocessor = gdix51c0_chicago_preprocessor_new (calibration, base, &error);
  enrollment = gdix51c0_chicago_enrollment_new ();
  if (!preprocessor || !enrollment)
    {
      fprintf (stderr, "could not initialize native enrollment: %s\n",
               error ? error->message : "allocation failed");
      return 1;
    }
  gdix51c0_chicago_engine_enrollment_policy_init (&policy);

  for (guint input = 5; input < (guint) argc; input++)
    {
      guint16 raw[GDIX51C0_CHICAGO_PIXELS];
      guint8 enhanced[GDIX51C0_CHICAGO_PIXELS];
      Gdix51c0ChicagoFeatureRecord records[
        GDIX51C0_CHICAGO_FEATURE_RECORD_LIMIT];
      Gdix51c0ChicagoMetricData metric_data;
      Gdix51c0ChicagoEnrollmentResult result;
      guint active_count = 0;
      guint record_count;
      guint position_reject = 0;
      guint8 quality;
      guint8 coverage;
      gboolean accepted;
      gboolean inserted;

      if (!read_raw16 (argv[input], raw))
        {
          fprintf (stderr, "could not read %s\n", argv[input]);
          return 1;
        }
      gdix51c0_chicago_preprocessor_build_enhanced (
        preprocessor, raw, enhanced);
      if (!dump_temporal_state (preprocessor, input - 5))
        {
          fprintf (stderr, "could not dump temporal state for %s\n",
                   argv[input]);
          return 1;
        }
      gdix51c0_chicago_preprocessor_finalize_metrics (
        gdix51c0_chicago_preprocessor_compute_base_quality_from_enhanced (
          enhanced),
        gdix51c0_chicago_preprocessor_compute_coverage (enhanced),
        &quality, &coverage);
      if (input - 5 < warmup_count)
        {
          printf ("input=%u file=%s action=identify-warmup q/c=%u/%u "
                  "enhanced=%016" G_GINT64_MODIFIER "x\n",
                  input - 4, argv[input], quality, coverage,
                  fnv1a64 (enhanced, sizeof (enhanced)));
          continue;
        }
      if (quality < MIN_QUALITY || coverage < MIN_COVERAGE)
        {
          printf ("input=%u file=%s admission=quality q/c=%u/%u\n",
                  input - 4, argv[input], quality, coverage);
          continue;
        }

      record_count = gdix51c0_chicago_feature_extract_subtemplate (
        enhanced, records, G_N_ELEMENTS (records), &active_count);
      if (record_count == 0)
        {
          printf ("input=%u file=%s admission=no-features q/c=%u/%u\n",
                  input - 4, argv[input], quality, coverage);
          continue;
        }
      gdix51c0_chicago_enrollment_build_metric_data (enhanced, &metric_data);
      if (!apply_native_resolution_map (
            preprocessor, raw, &metric_data, input - 5, &error))
        {
          fprintf (stderr, "could not apply resolution labels for %s: %s\n",
                   argv[input], error->message);
          return 1;
        }
      if (g_getenv ("NATIVE_RESOLUTION_TRACE"))
        {
          guint below_30 = 0;
          guint below_128 = 0;
          guint primary = 0;
          guint secondary = 0;
          guint validity = 0;
          guint coarse = 0;

          for (guint index = 0; index < G_N_ELEMENTS (enhanced); index++)
            {
              below_30 += enhanced[index] < 30;
              below_128 += enhanced[index] < 128;
            }
          for (guint index = 0; index < sizeof (metric_data.primary); index++)
            {
              primary += __builtin_popcount (metric_data.primary[index]);
              secondary += __builtin_popcount (metric_data.secondary[index]);
              validity += __builtin_popcount (metric_data.validity[index]);
            }
          for (guint index = 0; index < sizeof (metric_data.coarse_mask); index++)
            coarse += __builtin_popcount (metric_data.coarse_mask[index]);
          printf ("  resolution=%u below30=%u below128=%u maps=%u/%u/%u/%u "
                  "first-row-lsb=", metric_data.packed_resolution, below_30,
                  below_128, primary, secondary, validity, coarse);
          for (guint index = 0; index < GDIX51C0_CHICAGO_FEATURE_WIDTH; index++)
            putchar ((enhanced[index] & 1u) ? '1' : '0');
          putchar ('\n');
        }
      if (g_getenv ("NATIVE_RELATION_BUILD_TRACE"))
        for (guint subtemplate_index = 0;
             subtemplate_index < enrollment->subtemplates->len;
             subtemplate_index++)
          {
            const Gdix51c0ChicagoSubtemplate *old_subtemplate =
              g_ptr_array_index (enrollment->subtemplates,
                                 subtemplate_index);
            Gdix51c0ChicagoRelation raw_relation;
            gboolean raw_evidence;
            gint metric_a;
            gint metric_b;

            gdix51c0_chicago_enrollment_build_relation (
              old_subtemplate->records, old_subtemplate->record_count,
              &old_subtemplate->metric_data, records, record_count,
              &metric_data, &raw_relation, &metric_a, &metric_b,
              &raw_evidence);
            printf ("  raw-relation[%u]=%d,%d,%d,%d,%d,%d,%d "
                    "metric=%d/%d evidence=%u\n",
                    subtemplate_index, raw_relation.inlier_count,
                    raw_relation.transform[0], raw_relation.transform[1],
                    raw_relation.transform[2], raw_relation.transform[3],
                    raw_relation.transform[4], raw_relation.transform[5],
                    metric_a, metric_b, raw_evidence);
          }
      if (gdix51c0_chicago_enrollment_get_count (enrollment) == 0)
        inserted = gdix51c0_chicago_enrollment_insert_first (
          enrollment, records, record_count, active_count, quality, coverage,
          &metric_data, &result, &error);
      else
        inserted = gdix51c0_chicago_enrollment_insert_next (
          enrollment, records, record_count, active_count, quality, coverage,
          &metric_data, &result, &error);
      if (!inserted)
        {
          fprintf (stderr, "enrollment insertion failed for %s: %s\n",
                   argv[input], error ? error->message : "unknown error");
          return 1;
        }
      if (g_getenv ("NATIVE_RELATION_TRACE"))
        {
          const guint transform_count =
            gdix51c0_chicago_enrollment_get_transform_count (enrollment);

          for (guint relation_index = 0;
               relation_index < transform_count;
               relation_index++)
            {
              Gdix51c0ChicagoRelation relation;

              if (!gdix51c0_chicago_enrollment_get_relation (
                    enrollment, relation_index, &relation))
                continue;
              printf ("  relation[%u]=%d,%d,%d,%d,%d,%d,%d\n",
                      relation_index, relation.inlier_count,
                      relation.transform[0], relation.transform[1],
                      relation.transform[2], relation.transform[3],
                      relation.transform[4], relation.transform[5]);
            }
          for (guint subtemplate_index = 0;
               subtemplate_index <
                 gdix51c0_chicago_enrollment_get_count (enrollment);
               subtemplate_index++)
            {
              Gdix51c0ChicagoSubtemplateView view;

              if (gdix51c0_chicago_enrollment_get_subtemplate (
                    enrollment, subtemplate_index, &view))
                printf ("  subtemplate[%u] group=%u relation-base=%u\n",
                        subtemplate_index, view.group_state,
                        view.relation_base);
            }
          if (enrollment->has_group_anchor &&
              enrollment->subtemplates->len > enrollment->group_anchor)
            {
              const guint current = enrollment->subtemplates->len - 1;
              gint32 current_to_anchor[6];

              group_transform_between (enrollment, current,
                                       enrollment->group_anchor,
                                       current_to_anchor);
              printf ("  group-anchor=%u current-transform=%d,%d,%d,%d,%d,%d\n",
                      enrollment->group_anchor,
                      current_to_anchor[0], current_to_anchor[1],
                      current_to_anchor[2], current_to_anchor[3],
                      current_to_anchor[4], current_to_anchor[5]);
              for (guint candidate_index = 0;
                   candidate_index < current;
                   candidate_index++)
                {
                  gint32 current_to_candidate[6];
                  gint32 candidate_to_anchor[6];
                  gint32 candidate_path[6];

                  group_transform_between (enrollment, current,
                                           candidate_index,
                                           current_to_candidate);
                  if (candidate_index == enrollment->group_anchor)
                    memcpy (candidate_path, current_to_candidate,
                            sizeof (candidate_path));
                  else
                    {
                      group_transform_between (enrollment, candidate_index,
                                               enrollment->group_anchor,
                                               candidate_to_anchor);
                      compose_group_transform (candidate_to_anchor,
                                               current_to_candidate,
                                               candidate_path);
                    }
                  printf ("  group-candidate[%u]=%d,%d,%d,%d,%d,%d\n",
                          candidate_index, candidate_path[0],
                          candidate_path[1], candidate_path[2],
                          candidate_path[3], candidate_path[4],
                          candidate_path[5]);
                }
              for (guint subtemplate_index = 0;
                   subtemplate_index < enrollment->subtemplates->len;
                   subtemplate_index++)
                {
                  gint32 anchor_to_item[6];
                  gint32 transform[6];

                  if (subtemplate_index == enrollment->group_anchor)
                    continue;
                  group_transform_between (enrollment,
                                           enrollment->group_anchor,
                                           subtemplate_index,
                                           anchor_to_item);
                  compose_group_transform (anchor_to_item,
                                           current_to_anchor, transform);
                  printf ("  group-map[%u]=%d,%d,%d,%d,%d,%d\n",
                          subtemplate_index, transform[0], transform[1],
                          transform[2], transform[3], transform[4],
                          transform[5]);
                }
            }
        }
      accepted = gdix51c0_chicago_engine_enrollment_policy_accept (
        &policy, result.position_x, result.position_y, &position_reject);
      if (policy.defer_current_sample)
        {
          if (deferred_records != NULL)
            {
              fprintf (stderr, "duplicate deferred enrollment sample\n");
              return 1;
            }
          deferred_records = g_memdup2 (
            records, record_count * sizeof (*records));
          deferred_record_count = record_count;
          deferred_active_count = active_count;
          deferred_quality = quality;
          deferred_coverage = coverage;
          deferred_metric_data = metric_data;
          if (!gdix51c0_chicago_enrollment_drop_last (enrollment, &error))
            {
              fprintf (stderr, "could not defer enrollment sample: %s\n",
                       error->message);
              return 1;
            }
        }
      if (policy.restore_deferred_sample)
        {
          Gdix51c0ChicagoEnrollmentResult deferred_result;

          if (deferred_records == NULL)
            {
              fprintf (stderr, "missing deferred enrollment sample\n");
              return 1;
            }
          if (!gdix51c0_chicago_enrollment_insert_next (
                enrollment, deferred_records, deferred_record_count,
                deferred_active_count, deferred_quality, deferred_coverage,
                &deferred_metric_data, &deferred_result, &error))
            {
              fprintf (stderr, "could not restore enrollment sample: %s\n",
                       error->message);
              return 1;
            }
          g_clear_pointer (&deferred_records, g_free);
        }
      printf ("input=%u file=%s attempt=%u admission=ok records=%u "
              "active=%u q/c=%u/%u position=%u,%u policy=%s reject=%u "
              "used=%u tipped=%u retained=%u enhanced=%016"
              G_GINT64_MODIFIER "x\n",
              input - 4, argv[input], policy.touched, record_count,
              active_count, quality, coverage, result.position_x,
              result.position_y, accepted ? "accepted" : "retained",
              position_reject, policy.used, policy.tipped,
              gdix51c0_chicago_enrollment_get_count (enrollment),
              fnv1a64 (enhanced, sizeof (enhanced)));
    }

  packed = gdix51c0_chicago_enrollment_pack (enrollment, &error);
  if (!packed)
    {
      fprintf (stderr, "could not pack enrollment: %s\n", error->message);
      return 1;
    }
  {
    gconstpointer data;
    gsize size;

    data = g_bytes_get_data (packed, &size);
    if (!g_file_set_contents (argv[3], data, size, &error))
      {
        fprintf (stderr, "could not write %s: %s\n", argv[3], error->message);
        return 1;
      }
    printf ("complete=%u used=%u tipped=%u retained=%u packed=%zu out=%s\n",
            gdix51c0_chicago_engine_enrollment_policy_complete (&policy),
            policy.used, policy.tipped,
            gdix51c0_chicago_enrollment_get_count (enrollment), size, argv[3]);
  }
  return 0;
}
