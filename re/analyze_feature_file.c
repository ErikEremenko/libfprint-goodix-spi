// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

/* Print native Chicago feature counts for one enhanced 80x64 image. */

#include <stdio.h>

#include "../drivers/gdix51c0/gdix51c0-chicago-feature.c"

int
main (int   argc,
      char *argv[])
{
  guint8 enhanced[GDIX51C0_CHICAGO_FEATURE_PIXELS];
  Gdix51c0ChicagoFeatureRecord records[GDIX51C0_CHICAGO_FEATURE_RECORD_LIMIT];
  Gdix51c0ChicagoFeatureConsensus consensus;
  guint active_count = 0;
  FILE *file;
  guint record_count;

  if (argc != 2)
    return 2;
  file = fopen (argv[1], "rb");
  if (!file || fread (enhanced, 1, sizeof (enhanced), file) !=
               sizeof (enhanced))
    return 1;
  fclose (file);
  record_count = gdix51c0_chicago_feature_extract_subtemplate_full (
    enhanced, records, G_N_ELEMENTS (records), &active_count, &consensus);
  printf ("records=%u active=%u density=%u,%u,%u class=%u inactive=%u\n",
          record_count, active_count, consensus.negative_percent,
          consensus.positive_percent, consensus.neutral_percent,
          consensus.density_class, consensus.inactive_count);
  return 0;
}
