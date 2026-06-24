/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * boards/shields/dongle_screen/src/widgets/als.h
 *
 * Ambient light sensor lux display widget.
 * Shows the most recent lux reading from the ZMK ambient light service.
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_als {
    lv_obj_t *obj;
    sys_snode_t node;
};

int zmk_widget_als_init(struct zmk_widget_als *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_als_obj(struct zmk_widget_als *widget);
