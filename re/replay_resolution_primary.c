// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

/* Replay the recovered +0x3b5a0/+0x3c860 primary-plane boundary from dumped
 * 16-bit secondary and input-mask planes. RE tooling only. */

#include <stdio.h>

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

int
main (int   argc,
      char *argv[])
{
  guint16 secondary[GDIX51C0_CHICAGO_PIXELS];
  guint8 input_mask[GDIX51C0_CHICAGO_PIXELS];
  guint16 primary[GDIX51C0_CHICAGO_PIXELS];
  FILE *output;

  if (argc == 5 && strcmp (argv[1], "--filter") == 0)
    {
      if (!read_exact (argv[2], secondary, sizeof (secondary)) ||
          !read_exact (argv[3], input_mask, sizeof (input_mask)))
        return 2;
      gdix51c0_chicago_preprocessor_build_resolution_filtered_gradient (
        secondary, input_mask, primary);
      output = fopen (argv[4], "wb");
      if (!output || fwrite (primary, 1, sizeof (primary), output) !=
                     sizeof (primary))
        return 1;
      fclose (output);
      return 0;
    }

  if (argc == 5 && strcmp (argv[1], "--labels") == 0)
    {
      guint8 labels[GDIX51C0_CHICAGO_PIXELS];

      if (!read_exact (argv[2], secondary, sizeof (secondary)) ||
          !read_exact (argv[3], input_mask, sizeof (input_mask)))
        return 2;
      gdix51c0_chicago_preprocessor_build_resolution_map_labels (
        secondary, input_mask, labels);
      output = fopen (argv[4], "wb");
      if (!output || fwrite (labels, 1, sizeof (labels), output) !=
                     sizeof (labels))
        return 1;
      fclose (output);
      return 0;
    }

  if (argc == 4 && strcmp (argv[1], "--statistics") == 0)
    {
      Gdix51c0ChicagoResolutionStatistics statistics = { 0, };

      if (!read_exact (argv[2], secondary, sizeof (secondary)) ||
          !read_exact (argv[3], input_mask, sizeof (input_mask)))
        return 2;
      gdix51c0_chicago_preprocessor_calculate_resolution_primary_statistics (
        secondary, input_mask, &statistics);
      printf ("gradient-threshold=%d primary=%d,%d,%d,%d\n",
              gdix51c0_chicago_preprocessor_calculate_resolution_gradient_threshold (
                secondary, input_mask),
              statistics.primary_center_a, statistics.primary_center_b,
              statistics.primary_high_sample, statistics.primary_mid_sample);
      return 0;
    }

  if (argc == 4 && strcmp (argv[1], "--secondary-analysis") == 0)
    {
      Gdix51c0ChicagoResolutionSecondaryAnalysis analysis = { 0, };

      if (!read_exact (argv[2], secondary, sizeof (secondary)) ||
          !read_exact (argv[3], input_mask, sizeof (input_mask)))
        return 2;
      gdix51c0_chicago_preprocessor_calculate_resolution_secondary_analysis (
        secondary, input_mask, &analysis);
      printf ("upper=%d,%d lower=%d,%d upper-mid=%d peak=%d,%d\n",
              analysis.upper_cutoff, analysis.upper_state,
              analysis.lower_cutoff, analysis.lower_state,
              analysis.upper_mid_sample,
              analysis.peak_state, analysis.peak_value);
      return 0;
    }

  if (argc == 4 && strcmp (argv[1], "--branch-analysis") == 0)
    {
      Gdix51c0ChicagoResolutionSecondaryAnalysis analysis = { 0, };
      Gdix51c0ChicagoResolutionStatistics statistics = { 0, };
      gboolean use_exceptional;
      gboolean use_normal;
      gint branch_state;

      if (!read_exact (argv[2], secondary, sizeof (secondary)) ||
          !read_exact (argv[3], input_mask, sizeof (input_mask)))
        return 2;
      gdix51c0_chicago_preprocessor_calculate_resolution_secondary_analysis (
        secondary, input_mask, &analysis);
      gdix51c0_chicago_preprocessor_build_resolution_filtered_gradient (
        secondary, input_mask, primary);
      gdix51c0_chicago_preprocessor_calculate_resolution_primary_statistics (
        primary, input_mask, &statistics);
      gdix51c0_chicago_preprocessor_select_resolution_branches (
        &analysis, &statistics, &use_exceptional, &use_normal, &branch_state);
      printf ("exceptional=%d normal=%d branch=%d balance=%d,%d,%d,%d,%d "
              "gradient=%d,%d,%d,%d\n",
              use_exceptional, use_normal, branch_state,
              analysis.balance_center,
              analysis.upper_outer_mean, analysis.upper_inner_mean,
              analysis.lower_outer_mean, analysis.lower_inner_mean,
              statistics.primary_center_a, statistics.primary_center_b,
              statistics.primary_high_sample,
              statistics.primary_mid_sample);
      return 0;
    }

  if (argc != 7 ||
      !read_exact (argv[1], secondary, sizeof (secondary)) ||
      !read_exact (argv[2], input_mask, sizeof (input_mask)))
    {
      fprintf (stderr, "usage: %s <secondary16> <mask> <secondary-threshold> "
                       "<gradient-threshold> <exceptional> <output16>\n",
               argv[0]);
      fprintf (stderr, "       %s --secondary-analysis <secondary16> <mask>\n",
               argv[0]);
      fprintf (stderr, "       %s --branch-analysis <secondary16> <mask>\n",
               argv[0]);
      fprintf (stderr, "       %s --labels <secondary16> <mask> <output8>\n",
               argv[0]);
      return 2;
    }
  gdix51c0_chicago_preprocessor_build_resolution_primary_plane (
    secondary, input_mask, atoi (argv[3]), atoi (argv[4]),
    atoi (argv[5]) != 0, primary);
  output = fopen (argv[6], "wb");
  if (!output || fwrite (primary, 1, sizeof (primary), output) !=
                 sizeof (primary))
    {
      if (output)
        fclose (output);
      return 1;
    }
  fclose (output);
  return 0;
}
