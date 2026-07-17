// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

#include <glib.h>

#include "goodix-mcu-config.h"

static void
test_update_and_validate (void)
{
  guint8 config[] = { 0x70, 0x11, 0x74, 0x85, 0x00, 0x00 };

  g_assert_true (goodix_mcu_config_update_checksum (config, sizeof (config)));
  g_assert_cmphex (config[4], ==, 0x77);
  g_assert_cmphex (config[5], ==, 0xc3);
  g_assert_true (goodix_mcu_config_checksum_valid (config, sizeof (config)));

  config[0] ^= 1;
  g_assert_false (goodix_mcu_config_checksum_valid (config, sizeof (config)));
}

static void
test_invalid_shapes (void)
{
  guint8 config[3] = { 0 };

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*config_shape_valid*");
  g_assert_false (goodix_mcu_config_update_checksum (config, sizeof (config)));
  g_test_assert_expected_messages ();
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/goodix-mcu-config/update-and-validate",
                   test_update_and_validate);
  g_test_add_func ("/goodix-mcu-config/invalid-shapes", test_invalid_shapes);
  return g_test_run ();
}
