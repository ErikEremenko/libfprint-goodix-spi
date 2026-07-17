// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

#include <glib.h>
#include <string.h>

#include "goodix-command.h"

static void
assert_packet (guint8        command,
               const guint8 *payload,
               gsize         payload_size,
               const guint8 *expected,
               gsize         expected_size)
{
  guint8 packet[32];
  gsize packet_size = 0;

  g_assert_true (goodix_wire_encode_command (command, payload, payload_size,
                                             packet, sizeof (packet),
                                             &packet_size));
  g_assert_cmpuint (packet_size, ==, expected_size);
  g_assert_cmpmem (packet, packet_size, expected, expected_size);
}

static void
test_known_packets (void)
{
  guint8 payload[GOODIX_COMMAND_REGISTER_PAYLOAD_SIZE];
  static const guint8 reset_packet[] = { 0xa2, 0x03, 0x00, 0x01, 0x14, 0xf0 };
  static const guint8 idle_packet[] = { 0x70, 0x03, 0x00, 0x14, 0x00, 0x23 };
  static const guint8 read_packet[] = {
    0x82, 0x06, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x1e
  };

  goodix_command_encode_reset (TRUE, FALSE, 0x14, payload);
  assert_packet (GOODIX_COMMAND_RESET, payload,
                 GOODIX_COMMAND_RESET_PAYLOAD_SIZE,
                 reset_packet, sizeof (reset_packet));

  goodix_command_encode_idle (0x14, payload);
  assert_packet (GOODIX_COMMAND_MCU_SWITCH_TO_IDLE_MODE, payload,
                 GOODIX_COMMAND_IDLE_PAYLOAD_SIZE,
                 idle_packet, sizeof (idle_packet));

  goodix_command_encode_register_read (0, 4, payload);
  assert_packet (GOODIX_COMMAND_READ_SENSOR_REGISTER, payload,
                 GOODIX_COMMAND_REGISTER_PAYLOAD_SIZE,
                 read_packet, sizeof (read_packet));
}

static void
test_payload_layouts (void)
{
  guint8 payload[GOODIX_COMMAND_STORAGE_HEADER_SIZE];
  static const guint8 expected_nop[] = { 0, 0, 0, 0 };
  static const guint8 expected_default[] = { 1, 0 };
  static const guint8 expected_none[] = { 0, 0 };
  static const guint8 expected_write[] = { 0x00, 0x34, 0x12, 0xcd, 0xab };
  static const guint8 expected_storage[] = {
    0x03, 0x00, 0x02, 0xbb, 0x20, 0x00, 0x00, 0x00
  };

  goodix_command_encode_nop (payload);
  g_assert_cmpmem (payload, GOODIX_COMMAND_NOP_PAYLOAD_SIZE,
                   expected_nop, sizeof (expected_nop));

  goodix_command_encode_default (1, payload);
  g_assert_cmpmem (payload, GOODIX_COMMAND_DEFAULT_PAYLOAD_SIZE,
                   expected_default, sizeof (expected_default));

  goodix_command_encode_none (payload);
  g_assert_cmpmem (payload, GOODIX_COMMAND_NONE_PAYLOAD_SIZE,
                   expected_none, sizeof (expected_none));

  goodix_command_encode_register_write (0x1234, 0xabcd, payload);
  g_assert_cmpmem (payload, GOODIX_COMMAND_REGISTER_PAYLOAD_SIZE,
                   expected_write, sizeof (expected_write));

  goodix_command_encode_storage_header (0xbb020003, 32, payload);
  g_assert_cmpmem (payload, GOODIX_COMMAND_STORAGE_HEADER_SIZE,
                   expected_storage, sizeof (expected_storage));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/goodix-command/known-packets", test_known_packets);
  g_test_add_func ("/goodix-command/payload-layouts", test_payload_layouts);
  return g_test_run ();
}
