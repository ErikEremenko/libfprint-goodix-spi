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

#include "goodix-sensor-profile.h"

static const GoodixSensorProfile profiles[] = {
  {
    .chip_id = 0x2504,
    .sensor_type = 12,
    .width = 80,
    .height = 64,
    .otp_size = 64,
    .fdt_trigger_min = 5,
    .fdt_trigger_max = 6,
    .algorithm = GOODIX_SENSOR_ALGORITHM_CHICAGO_HS,
    .algorithm_name = "ChicagoHS",
  },
};

const GoodixSensorProfile *
goodix_sensor_profile_lookup (guint32 chip_id)
{
  for (guint index = 0; index < G_N_ELEMENTS (profiles); index++)
    if (profiles[index].chip_id == chip_id)
      return &profiles[index];

  return NULL;
}
