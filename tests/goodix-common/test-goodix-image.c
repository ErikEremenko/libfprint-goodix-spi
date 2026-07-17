// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

#include <glib.h>

#include "goodix-image.h"

static void
test_raw12_group (void)
{
  const guint8 packed[] = { 0x61, 0x23, 0x89, 0x45, 0xab, 0xc7 };
  const guint16 expected[] = { 0x123, 0x456, 0x789, 0xabc };
  guint16 pixels[G_N_ELEMENTS (expected)] = { 0, };

  g_assert_cmpuint (goodix_raw12_decode (
                      packed, sizeof (packed), pixels,
                      G_N_ELEMENTS (pixels)), ==, G_N_ELEMENTS (expected));
  g_assert_cmpmem (pixels, sizeof (pixels), expected, sizeof (expected));
}

static void
test_raw12_bounds (void)
{
  const guint8 packed[] = {
    0x61, 0x23, 0x89, 0x45, 0xab, 0xc7,
    0xff, 0xff, 0xff, 0xff, 0xff,
  };
  guint16 pixels[8] = { 0, };

  g_assert_cmpuint (goodix_raw12_decode (
                      packed, sizeof (packed), pixels,
                      G_N_ELEMENTS (pixels)), ==, 4);
  g_assert_cmphex (pixels[0], ==, 0x123);
  g_assert_cmphex (pixels[3], ==, 0xabc);
  g_assert_cmphex (pixels[4], ==, 0);
  g_assert_cmpuint (goodix_raw12_decode (
                      packed, sizeof (packed), pixels, 3), ==, 0);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/goodix-image/raw12-group", test_raw12_group);
  g_test_add_func ("/goodix-image/raw12-bounds", test_raw12_bounds);
  return g_test_run ();
}
