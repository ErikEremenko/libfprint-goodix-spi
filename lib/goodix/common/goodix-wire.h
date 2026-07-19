// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Transport-neutral Goodix wire framing.
 *
 * Copyright (C) 2021 Alexander Meiler <alex.meiler@protonmail.com>
 * Copyright (C) 2021 Matthieu CHARETTE <matthieu.charette@gmail.com>
 * Copyright (C) 2021 Natasha England-Elbro <ashenglandelbro@protonmail.com>
 * Copyright (C) 2024 goodix-fp-linux-dev contributors
 * Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define GOODIX_WIRE_OUTER_HEADER_SIZE 4u
#define GOODIX_WIRE_COMMAND_HEADER_SIZE 3u
#define GOODIX_WIRE_COMMAND_OVERHEAD 4u
#define GOODIX_WIRE_NULL_CHECKSUM 0x88u

/* Base command byte: high nibble is category, bits 1..3 are operation.
 * USB implementations use bit 0 as a continuation marker; it is not part of
 * the base command identity. */
#define GOODIX_WIRE_COMMAND_ID(category, operation) \
  ((guint8) ((((category) & 0x0fu) << 4) | (((operation) & 0x07u) << 1)))
#define GOODIX_WIRE_COMMAND_CATEGORY(command_id) \
  ((guint8) ((command_id) >> 4))
#define GOODIX_WIRE_COMMAND_OPERATION(command_id) \
  ((guint8) (((command_id) >> 1) & 0x07u))
#define GOODIX_WIRE_COMMAND_CONTINUATION(command_id) \
  (((command_id) & 0x01u) != 0)

/* The command checksum makes the complete command packet sum to 0xaa. */
guint8 goodix_wire_checksum (const guint8 *data,
                             gsize         length);

void goodix_wire_encode_outer_header (
  guint8  flags,
  guint16 payload_length,
  guint8  header[GOODIX_WIRE_OUTER_HEADER_SIZE]);

gboolean goodix_wire_decode_outer_header (
  const guint8 header[GOODIX_WIRE_OUTER_HEADER_SIZE],
  guint8      *flags,
  guint16     *payload_length);

gboolean goodix_wire_encode_command (guint8        command,
                                     const guint8 *payload,
                                     gsize         payload_length,
                                     guint8       *output,
                                     gsize         output_size,
                                     gsize        *encoded_size);

gboolean goodix_wire_validate_command (const guint8 *packet,
                                       gsize         packet_size,
                                       gboolean      allow_null_checksum,
                                       gsize        *payload_length);

/* Validate a complete command/response packet and return borrowed views into
 * it. The returned payload remains owned by @packet. */
gboolean goodix_wire_decode_command (const guint8  *packet,
                                     gsize          packet_size,
                                     gboolean       allow_null_checksum,
                                     guint8        *command,
                                     const guint8 **payload,
                                     gsize         *payload_length);

G_END_DECLS
