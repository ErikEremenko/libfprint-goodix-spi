// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Transport-neutral payloads for commands shared by Goodix 5xx devices.
 *
 * Copyright (C) 2021 Alexander Meiler <alex.meiler@protonmail.com>
 * Copyright (C) 2021 Matthieu CHARETTE <matthieu.charette@gmail.com>
 * Copyright (C) 2021 Natasha England-Elbro <ashenglandelbro@protonmail.com>
 * Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#pragma once

#include <glib.h>

#include "goodix-wire.h"

G_BEGIN_DECLS

typedef enum
{
  GOODIX_COMMAND_NOP                          = GOODIX_WIRE_COMMAND_ID (0x0, 0x0),
  GOODIX_COMMAND_MCU_GET_IMAGE                = GOODIX_WIRE_COMMAND_ID (0x2, 0x0),
  GOODIX_COMMAND_MCU_SWITCH_TO_FDT_DOWN       = GOODIX_WIRE_COMMAND_ID (0x3, 0x1),
  GOODIX_COMMAND_MCU_SWITCH_TO_FDT_UP         = GOODIX_WIRE_COMMAND_ID (0x3, 0x2),
  GOODIX_COMMAND_MCU_SWITCH_TO_FDT_MODE       = GOODIX_WIRE_COMMAND_ID (0x3, 0x3),
  GOODIX_COMMAND_NAV                          = GOODIX_WIRE_COMMAND_ID (0x5, 0x0),
  GOODIX_COMMAND_MCU_SWITCH_TO_IDLE_MODE      = GOODIX_WIRE_COMMAND_ID (0x7, 0x0),
  GOODIX_COMMAND_WRITE_SENSOR_REGISTER        = GOODIX_WIRE_COMMAND_ID (0x8, 0x0),
  GOODIX_COMMAND_READ_SENSOR_REGISTER         = GOODIX_WIRE_COMMAND_ID (0x8, 0x1),
  GOODIX_COMMAND_UPLOAD_CONFIG_MCU            = GOODIX_WIRE_COMMAND_ID (0x9, 0x0),
  GOODIX_COMMAND_SET_POWERDOWN_SCAN_FREQUENCY = GOODIX_WIRE_COMMAND_ID (0x9, 0x2),
  GOODIX_COMMAND_ENABLE_CHIP                  = GOODIX_WIRE_COMMAND_ID (0x9, 0x3),
  GOODIX_COMMAND_RESET                        = GOODIX_WIRE_COMMAND_ID (0xa, 0x1),
  GOODIX_COMMAND_READ_OTP                     = GOODIX_WIRE_COMMAND_ID (0xa, 0x3),
  GOODIX_COMMAND_FIRMWARE_VERSION             = GOODIX_WIRE_COMMAND_ID (0xa, 0x4),
  GOODIX_COMMAND_QUERY_MCU_STATE              = GOODIX_WIRE_COMMAND_ID (0xa, 0x7),
  GOODIX_COMMAND_ACK                          = GOODIX_WIRE_COMMAND_ID (0xb, 0x0),
  GOODIX_COMMAND_REQUEST_TLS_CONNECTION       = GOODIX_WIRE_COMMAND_ID (0xd, 0x0),
  GOODIX_COMMAND_TLS_ESTABLISHED              = GOODIX_WIRE_COMMAND_ID (0xd, 0x2),
  GOODIX_COMMAND_PRESET_PSK_WRITE             = GOODIX_WIRE_COMMAND_ID (0xe, 0x0),
  GOODIX_COMMAND_PRESET_PSK_READ              = GOODIX_WIRE_COMMAND_ID (0xe, 0x2),
} GoodixCommandId;

#define GOODIX_COMMAND_NOP_PAYLOAD_SIZE            4u
#define GOODIX_COMMAND_DEFAULT_PAYLOAD_SIZE        2u
#define GOODIX_COMMAND_NONE_PAYLOAD_SIZE           2u
#define GOODIX_COMMAND_IDLE_PAYLOAD_SIZE           2u
#define GOODIX_COMMAND_RESET_PAYLOAD_SIZE          2u
#define GOODIX_COMMAND_REGISTER_PAYLOAD_SIZE       5u
#define GOODIX_COMMAND_POWERDOWN_PAYLOAD_SIZE      2u
#define GOODIX_COMMAND_ENABLE_CHIP_PAYLOAD_SIZE    2u
#define GOODIX_COMMAND_STORAGE_HEADER_SIZE         8u

void goodix_command_encode_nop (
  guint8 payload[GOODIX_COMMAND_NOP_PAYLOAD_SIZE]);

void goodix_command_encode_default (
  guint8 unused_flags,
  guint8 payload[GOODIX_COMMAND_DEFAULT_PAYLOAD_SIZE]);

void goodix_command_encode_none (
  guint8 payload[GOODIX_COMMAND_NONE_PAYLOAD_SIZE]);

void goodix_command_encode_idle (
  guint8 sleep_time,
  guint8 payload[GOODIX_COMMAND_IDLE_PAYLOAD_SIZE]);

void goodix_command_encode_reset (
  gboolean reset_sensor,
  gboolean soft_reset_mcu,
  guint8   sleep_time,
  guint8   payload[GOODIX_COMMAND_RESET_PAYLOAD_SIZE]);

void goodix_command_encode_register_read (
  guint16 address,
  guint8  length,
  guint8  payload[GOODIX_COMMAND_REGISTER_PAYLOAD_SIZE]);

void goodix_command_encode_register_write (
  guint16 address,
  guint16 value,
  guint8  payload[GOODIX_COMMAND_REGISTER_PAYLOAD_SIZE]);

void goodix_command_encode_powerdown_frequency (
  guint16 frequency,
  guint8  payload[GOODIX_COMMAND_POWERDOWN_PAYLOAD_SIZE]);

void goodix_command_encode_enable_chip (
  gboolean enable,
  guint8   payload[GOODIX_COMMAND_ENABLE_CHIP_PAYLOAD_SIZE]);

void goodix_command_encode_storage_header (
  guint32 type,
  guint32 length,
  guint8  payload[GOODIX_COMMAND_STORAGE_HEADER_SIZE]);

G_END_DECLS
