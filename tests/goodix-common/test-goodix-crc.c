// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

#include <glib.h>

#include "goodix-crc.h"

static void
test_crc32_mpeg2_check (void)
{
  static const guint8 check[] = "123456789";

  g_assert_cmphex (goodix_crc32_mpeg2 (check, sizeof (check) - 1),
                   ==, 0x0376e6e7u);
  g_assert_cmphex (goodix_crc32_mpeg2 (NULL, 0), ==, G_MAXUINT32);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/goodix-crc/crc32-mpeg2", test_crc32_mpeg2_check);
  return g_test_run ();
}
