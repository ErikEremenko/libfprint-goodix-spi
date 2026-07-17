// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

#include <glib.h>

#include "goodix-sensor-profile.h"

static void
test_gdix51c0_profile (void)
{
  const GoodixSensorProfile *profile = goodix_sensor_profile_lookup (0x2504);

  g_assert_nonnull (profile);
  g_assert_cmphex (profile->chip_id, ==, 0x2504);
  g_assert_cmpuint (profile->sensor_type, ==, 12);
  g_assert_cmpuint (profile->width, ==, 80);
  g_assert_cmpuint (profile->height, ==, 64);
  g_assert_cmpuint (profile->otp_size, ==, 64);
  g_assert_cmpuint (profile->fdt_trigger_min, ==, 5);
  g_assert_cmpuint (profile->fdt_trigger_max, ==, 6);
  g_assert_cmpint (profile->algorithm, ==,
                   GOODIX_SENSOR_ALGORITHM_CHICAGO_HS);
  g_assert_cmpstr (profile->algorithm_name, ==, "ChicagoHS");
}

static void
test_unknown_profile (void)
{
  g_assert_null (goodix_sensor_profile_lookup (0));
  g_assert_null (goodix_sensor_profile_lookup (0xffffffff));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/goodix-sensor-profile/gdix51c0",
                   test_gdix51c0_profile);
  g_test_add_func ("/goodix-sensor-profile/unknown", test_unknown_profile);
  return g_test_run ();
}
