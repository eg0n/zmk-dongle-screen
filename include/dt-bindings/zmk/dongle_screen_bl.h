/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * include/dt-bindings/zmk/dongle_screen_bl.h
 *
 * Dongle screen brightness behavior command codes.
 *
 * Keymap usage:
 *   &ds_bl DS_BL_INC     increase brightness by one step
 *   &ds_bl DS_BL_DEC     decrease brightness by one step
 *   &ds_bl DS_BL_TOG     toggle on / off
 *   &ds_bl DS_BL_SET 75  set to 75%  (param2 = percent 0-100)
 */

#pragma once

#define DS_BL_INC_CMD 0
#define DS_BL_DEC_CMD 1
#define DS_BL_TOG_CMD 2
#define DS_BL_SET_CMD 3

/* Convenience macros — param2 unused except for DS_BL_SET */
#define DS_BL_INC DS_BL_INC_CMD 0
#define DS_BL_DEC DS_BL_DEC_CMD 0
#define DS_BL_TOG DS_BL_TOG_CMD 0
#define DS_BL_SET DS_BL_SET_CMD
