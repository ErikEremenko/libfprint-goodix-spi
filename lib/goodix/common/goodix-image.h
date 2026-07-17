// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Transport-neutral Goodix image primitives.
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

#define GOODIX_RAW12_GROUP_BYTES  6u
#define GOODIX_RAW12_GROUP_PIXELS 4u

/* Decode the Goodix 6-byte/4-pixel raw12 packing in stream order. Returns the
 * number of pixels written. A final partial group is ignored. */
gsize goodix_raw12_decode (const guint8 *packed,
                           gsize         packed_length,
                           guint16      *pixels,
                           gsize         pixel_capacity);

G_END_DECLS
