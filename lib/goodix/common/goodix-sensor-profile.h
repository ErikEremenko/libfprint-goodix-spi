// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Transport-neutral Goodix sensor identity and algorithm profiles.
 *
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

typedef enum {
  GOODIX_SENSOR_ALGORITHM_UNKNOWN = 0,
  GOODIX_SENSOR_ALGORITHM_CHICAGO_HS,
} GoodixSensorAlgorithm;

typedef struct {
  guint32               chip_id;
  guint8                sensor_type;
  guint16               width;
  guint16               height;
  guint16               otp_size;
  guint8                fdt_trigger_min;
  guint8                fdt_trigger_max;
  GoodixSensorAlgorithm algorithm;
  const gchar          *algorithm_name;
} GoodixSensorProfile;

const GoodixSensorProfile *goodix_sensor_profile_lookup (guint32 chip_id);

G_END_DECLS
