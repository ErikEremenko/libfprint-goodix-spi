// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

#include <glib.h>
#include <string.h>

#include "goodix-wire.h"

static void
test_outer_header (void)
{
  guint8 header[GOODIX_WIRE_OUTER_HEADER_SIZE];
  guint8 flags;
  guint16 payload_length;
  const guint8 expected[] = { 0xa0, 0x34, 0x12, 0xe6 };

  goodix_wire_encode_outer_header (0xa0, 0x1234, header);
  g_assert_cmpmem (header, sizeof (header), expected, sizeof (expected));
  g_assert_true (goodix_wire_decode_outer_header (
    header, &flags, &payload_length));
  g_assert_cmphex (flags, ==, 0xa0);
  g_assert_cmphex (payload_length, ==, 0x1234);

  header[3] ^= 1;
  g_assert_false (goodix_wire_decode_outer_header (header, NULL, NULL));
}

static void
test_command (void)
{
  const guint8 payload[] = { 0x00, 0x00 };
  const guint8 expected[] = { 0x32, 0x03, 0x00, 0x00, 0x00, 0x75 };
  guint8 packet[sizeof (expected)];
  gsize encoded_size;
  gsize payload_length;

  g_assert_true (goodix_wire_encode_command (
    0x32, payload, sizeof (payload), packet, sizeof (packet), &encoded_size));
  g_assert_cmpuint (encoded_size, ==, sizeof (expected));
  g_assert_cmpmem (packet, sizeof (packet), expected, sizeof (expected));
  g_assert_true (goodix_wire_validate_command (
    packet, sizeof (packet), FALSE, &payload_length));
  g_assert_cmpuint (payload_length, ==, sizeof (payload));

  packet[sizeof (packet) - 1] ^= 1;
  g_assert_false (goodix_wire_validate_command (
    packet, sizeof (packet), FALSE, NULL));
}

static void
test_null_checksum (void)
{
  guint8 packet[] = { 0xd0, 0x01, 0x00, GOODIX_WIRE_NULL_CHECKSUM };

  g_assert_false (goodix_wire_validate_command (
    packet, sizeof (packet), FALSE, NULL));
  g_assert_true (goodix_wire_validate_command (
    packet, sizeof (packet), TRUE, NULL));
}

static void
test_decode_command (void)
{
  /* Register-zero response on GDIX51C0: status plus LE chip id 0x2504. */
  const guint8 packet[] = {
    0x82, 0x05, 0x00, 0xa2, 0x04, 0x25, 0x00, 0x58,
  };
  const guint8 *payload;
  guint8 command;
  gsize payload_length;

  g_assert_true (goodix_wire_decode_command (
    packet, sizeof (packet), FALSE, &command, &payload, &payload_length));
  g_assert_cmphex (command, ==, 0x82);
  g_assert_cmpuint (payload_length, ==, 4);
  g_assert_cmpmem (payload, payload_length, packet + 3, 4);
  g_assert_cmphex ((guint16) payload[1] | ((guint16) payload[2] << 8),
                   ==, 0x2504);

  g_assert_false (goodix_wire_decode_command (
    packet, sizeof (packet) - 1, FALSE, NULL, NULL, NULL));
}

static void
test_command_bounds (void)
{
  guint8 packet[GOODIX_WIRE_COMMAND_OVERHEAD];
  const guint8 payload = 0;

  g_assert_false (goodix_wire_encode_command (
    0x20, &payload, 1, packet, sizeof (packet), NULL));
  g_assert_false (goodix_wire_encode_command (
    0x20, NULL, 1, packet, sizeof (packet), NULL));
  g_assert_false (goodix_wire_encode_command (
    0x20, &payload, G_MAXUINT16, packet, sizeof (packet), NULL));
  memset (packet, 0, sizeof (packet));
  g_assert_false (goodix_wire_validate_command (
    packet, sizeof (packet), TRUE, NULL));
  packet[1] = 2;
  g_assert_false (goodix_wire_validate_command (
    packet, sizeof (packet), TRUE, NULL));
}

static void
test_command_id (void)
{
  guint8 fdt_down = GOODIX_WIRE_COMMAND_ID (0x3, 0x1);
  guint8 continuation = fdt_down | 1;

  g_assert_cmphex (fdt_down, ==, 0x32);
  g_assert_cmphex (GOODIX_WIRE_COMMAND_ID (0x2, 0x0), ==, 0x20);
  g_assert_cmphex (GOODIX_WIRE_COMMAND_ID (0xa, 0x4), ==, 0xa8);
  g_assert_cmpuint (GOODIX_WIRE_COMMAND_CATEGORY (fdt_down), ==, 0x3);
  g_assert_cmpuint (GOODIX_WIRE_COMMAND_OPERATION (fdt_down), ==, 0x1);
  g_assert_false (GOODIX_WIRE_COMMAND_CONTINUATION (fdt_down));
  g_assert_true (GOODIX_WIRE_COMMAND_CONTINUATION (continuation));
  g_assert_cmpuint (GOODIX_WIRE_COMMAND_CATEGORY (continuation), ==, 0x3);
  g_assert_cmpuint (GOODIX_WIRE_COMMAND_OPERATION (continuation), ==, 0x1);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/goodix-wire/outer-header", test_outer_header);
  g_test_add_func ("/goodix-wire/command", test_command);
  g_test_add_func ("/goodix-wire/null-checksum", test_null_checksum);
  g_test_add_func ("/goodix-wire/decode-command", test_decode_command);
  g_test_add_func ("/goodix-wire/bounds", test_command_bounds);
  g_test_add_func ("/goodix-wire/command-id", test_command_id);
  return g_test_run ();
}
