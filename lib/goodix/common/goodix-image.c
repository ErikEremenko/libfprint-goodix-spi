// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Transport-neutral Goodix image primitives.
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

#include "goodix-image.h"

gsize
goodix_raw12_decode (const guint8 *packed,
                     gsize         packed_length,
                     guint16      *pixels,
                     gsize         pixel_capacity)
{
  gsize groups;

  g_return_val_if_fail (packed != NULL || packed_length == 0, 0);
  g_return_val_if_fail (pixels != NULL || pixel_capacity == 0, 0);

  groups = MIN (packed_length / GOODIX_RAW12_GROUP_BYTES,
                pixel_capacity / GOODIX_RAW12_GROUP_PIXELS);
  for (gsize group = 0; group < groups; group++)
    {
      const guint8 *source = packed + group * GOODIX_RAW12_GROUP_BYTES;
      guint16 *target = pixels + group * GOODIX_RAW12_GROUP_PIXELS;

      target[0] = ((guint16) (source[0] & 0x0f) << 8) | source[1];
      target[1] = ((guint16) source[3] << 4) | (source[0] >> 4);
      target[2] = ((guint16) (source[5] & 0x0f) << 8) | source[2];
      target[3] = ((guint16) source[4] << 4) | (source[5] >> 4);
    }

  return groups * GOODIX_RAW12_GROUP_PIXELS;
}
