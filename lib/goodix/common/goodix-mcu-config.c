// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Checksum shared by Goodix MCU configuration blobs.
 *
 * Copyright (C) 2024 goodix-fp-linux-dev contributors
 * Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "goodix-mcu-config.h"

static gboolean
config_shape_valid (const guint8 *config,
                    gsize         length)
{
  return config != NULL && length >= 2 && (length % 2) == 0;
}

static guint16
calculate_checksum (const guint8 *config,
                    gsize         length)
{
  guint32 sum = 0xa5a5;

  for (gsize offset = 0; offset < length - 2; offset += 2)
    sum += (guint16) config[offset] |
           ((guint16) config[offset + 1] << 8);

  return (guint16) (0u - sum);
}

gboolean
goodix_mcu_config_checksum_valid (const guint8 *config,
                                  gsize         length)
{
  guint16 stored;

  g_return_val_if_fail (config_shape_valid (config, length), FALSE);
  stored = (guint16) config[length - 2] |
           ((guint16) config[length - 1] << 8);
  return stored == calculate_checksum (config, length);
}

gboolean
goodix_mcu_config_update_checksum (guint8 *config,
                                   gsize   length)
{
  guint16 checksum;

  g_return_val_if_fail (config_shape_valid (config, length), FALSE);
  checksum = calculate_checksum (config, length);
  config[length - 2] = (guint8) checksum;
  config[length - 1] = (guint8) (checksum >> 8);
  return TRUE;
}
