/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_connection {
    lv_obj_t *obj;
    sys_snode_t node;
};

int zmk_widget_connection_init(struct zmk_widget_connection *widget,
                               lv_obj_t *parent);
lv_obj_t *zmk_widget_connection_obj(struct zmk_widget_connection *widget);
