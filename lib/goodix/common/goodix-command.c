// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Transport-neutral payloads for commands shared by Goodix 5xx devices.
 *
 * Copyright (C) 2021 Alexander Meiler <alex.meiler@protonmail.com>
 * Copyright (C) 2021 Matthieu CHARETTE <matthieu.charette@gmail.com>
 * Copyright (C) 2021 Natasha England-Elbro <natasha@natashaee.me>
 * Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "goodix-command.h"

#include <string.h>

static void
write_le16 (guint8 *output,
            guint16 value)
{
  output[0] = (guint8) value;
  output[1] = (guint8) (value >> 8);
}

static void
write_le32 (guint8 *output,
            guint32 value)
{
  output[0] = (guint8) value;
  output[1] = (guint8) (value >> 8);
  output[2] = (guint8) (value >> 16);
  output[3] = (guint8) (value >> 24);
}

void
goodix_command_encode_nop (
  guint8 payload[GOODIX_COMMAND_NOP_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  memset (payload, 0, GOODIX_COMMAND_NOP_PAYLOAD_SIZE);
}

void
goodix_command_encode_default (
  guint8 unused_flags,
  guint8 payload[GOODIX_COMMAND_DEFAULT_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  payload[0] = unused_flags;
  payload[1] = 0;
}

void
goodix_command_encode_none (
  guint8 payload[GOODIX_COMMAND_NONE_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  payload[0] = 0;
  payload[1] = 0;
}

void
goodix_command_encode_idle (
  guint8 sleep_time,
  guint8 payload[GOODIX_COMMAND_IDLE_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  payload[0] = sleep_time;
  payload[1] = 0;
}

void
goodix_command_encode_reset (
  gboolean reset_sensor,
  gboolean soft_reset_mcu,
  guint8   sleep_time,
  guint8   payload[GOODIX_COMMAND_RESET_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  payload[0] = (reset_sensor ? 0x01 : 0) |
               (soft_reset_mcu ? 0x02 : 0);
  payload[1] = sleep_time;
}

void
goodix_command_encode_register_read (
  guint16 address,
  guint8  length,
  guint8  payload[GOODIX_COMMAND_REGISTER_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  payload[0] = 0;
  write_le16 (payload + 1, address);
  payload[3] = length;
  payload[4] = 0;
}

void
goodix_command_encode_register_write (
  guint16 address,
  guint16 value,
  guint8  payload[GOODIX_COMMAND_REGISTER_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  payload[0] = 0;
  write_le16 (payload + 1, address);
  write_le16 (payload + 3, value);
}

void
goodix_command_encode_powerdown_frequency (
  guint16 frequency,
  guint8  payload[GOODIX_COMMAND_POWERDOWN_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  write_le16 (payload, frequency);
}

void
goodix_command_encode_enable_chip (
  gboolean enable,
  guint8   payload[GOODIX_COMMAND_ENABLE_CHIP_PAYLOAD_SIZE])
{
  g_return_if_fail (payload != NULL);
  payload[0] = enable ? 1 : 0;
  payload[1] = 0;
}

void
goodix_command_encode_storage_header (
  guint32 type,
  guint32 length,
  guint8  payload[GOODIX_COMMAND_STORAGE_HEADER_SIZE])
{
  g_return_if_fail (payload != NULL);
  write_le32 (payload, type);
  write_le32 (payload + 4, length);
}
