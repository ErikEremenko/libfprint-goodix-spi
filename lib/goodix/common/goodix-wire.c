// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Transport-neutral Goodix wire framing.
 *
 * Copyright (C) 2021 Alexander Meiler <alex.meiler@protonmail.com>
 * Copyright (C) 2021 Matthieu CHARETTE <matthieu.charette@gmail.com>
 * Copyright (C) 2021 Natasha England-Elbro <natasha@natashaee.me>
 * Copyright (C) 2024 goodix-fp-linux-dev contributors
 * Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "goodix-wire.h"

#include <string.h>

guint8
goodix_wire_checksum (const guint8 *data,
                      gsize         length)
{
  guint sum = 0;

  g_return_val_if_fail (data != NULL || length == 0, 0);
  for (gsize index = 0; index < length; index++)
    sum += data[index];
  return (guint8) (0xaa - sum);
}

void
goodix_wire_encode_outer_header (
  guint8  flags,
  guint16 payload_length,
  guint8  header[GOODIX_WIRE_OUTER_HEADER_SIZE])
{
  g_return_if_fail (header != NULL);
  header[0] = flags;
  header[1] = (guint8) payload_length;
  header[2] = (guint8) (payload_length >> 8);
  header[3] = (guint8) (header[0] + header[1] + header[2]);
}

gboolean
goodix_wire_decode_outer_header (
  const guint8 header[GOODIX_WIRE_OUTER_HEADER_SIZE],
  guint8      *flags,
  guint16     *payload_length)
{
  g_return_val_if_fail (header != NULL, FALSE);
  if ((guint8) (header[0] + header[1] + header[2]) != header[3])
    return FALSE;
  if (flags != NULL)
    *flags = header[0];
  if (payload_length != NULL)
    *payload_length = (guint16) header[1] | ((guint16) header[2] << 8);
  return TRUE;
}

gboolean
goodix_wire_encode_command (guint8        command,
                            const guint8 *payload,
                            gsize         payload_length,
                            guint8       *output,
                            gsize         output_size,
                            gsize        *encoded_size)
{
  const gsize total = payload_length + GOODIX_WIRE_COMMAND_OVERHEAD;
  const guint16 wire_length = (guint16) (payload_length + 1);

  g_return_val_if_fail (output != NULL, FALSE);
  if ((payload == NULL && payload_length != 0) ||
      payload_length > G_MAXUINT16 - 1 || output_size < total)
    return FALSE;

  output[0] = command;
  output[1] = (guint8) wire_length;
  output[2] = (guint8) (wire_length >> 8);
  if (payload_length > 0)
    memcpy (output + GOODIX_WIRE_COMMAND_HEADER_SIZE,
            payload, payload_length);
  output[total - 1] = goodix_wire_checksum (output, total - 1);
  if (encoded_size != NULL)
    *encoded_size = total;
  return TRUE;
}

gboolean
goodix_wire_validate_command (const guint8 *packet,
                              gsize         packet_size,
                              gboolean      allow_null_checksum,
                              gsize        *payload_length)
{
  guint16 wire_length;
  gsize total;
  guint8 checksum;

  g_return_val_if_fail (packet != NULL, FALSE);
  if (packet_size < GOODIX_WIRE_COMMAND_OVERHEAD)
    return FALSE;
  wire_length = (guint16) packet[1] | ((guint16) packet[2] << 8);
  if (wire_length < 1)
    return FALSE;
  total = GOODIX_WIRE_COMMAND_HEADER_SIZE + wire_length;
  if (packet_size < total)
    return FALSE;
  checksum = packet[total - 1];
  if (checksum != goodix_wire_checksum (packet, total - 1) &&
      !(allow_null_checksum && checksum == GOODIX_WIRE_NULL_CHECKSUM))
    return FALSE;
  if (payload_length != NULL)
    *payload_length = wire_length - 1;
  return TRUE;
}

gboolean
goodix_wire_decode_command (const guint8  *packet,
                            gsize          packet_size,
                            gboolean       allow_null_checksum,
                            guint8        *command,
                            const guint8 **payload,
                            gsize         *payload_length)
{
  gsize decoded_length;

  if (!goodix_wire_validate_command (packet, packet_size,
                                     allow_null_checksum, &decoded_length))
    return FALSE;

  if (command != NULL)
    *command = packet[0];
  if (payload != NULL)
    *payload = packet + GOODIX_WIRE_COMMAND_HEADER_SIZE;
  if (payload_length != NULL)
    *payload_length = decoded_length;
  return TRUE;
}
