/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * boards/shields/dongle_screen/src/widgets/als.c
 *
 * Ambient light sensor lux display widget for zmk-dongle-screen.
 *
 * Displays the most recent lux value from zmk_ambient_light_get_lux().
 * Updates are driven by the ZMK ambient light callback (same mechanism
 * used by brightness.c), dispatched safely onto the LVGL display work
 * queue via zmk_display_work_q().
 *
 * Layout (horizontal screen):
 *
 *   ┌─────────────────────┐
 *   │  ☀  │  1234 lx      │
 *   └─────────────────────┘
 *    icon    numeric value
 *
 * Width: 2 × GRID_CELL_WIDTH (90 px)   Height: GRID_CELL_HEIGHT (30 px)
 *
 * Add to custom_status_screen.c:
 *   #if CONFIG_DONGLE_SCREEN_ALS_ACTIVE
 *   #include "widgets/als.h"
 *   static struct zmk_widget_als als_widget;
 *   ...
 *   zmk_widget_als_init(&als_widget, screen);
 *   lv_obj_align(zmk_widget_als_obj(&als_widget), LV_ALIGN_TOP_MID, 0, 5);
 *   #endif
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/display.h>
#include <zmk/ambient_light.h>

#include <fonts.h>
#include <material_32.h>

#include "als.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

struct als_state {
    uint32_t lux;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* ------------------------------------------------------------------ */
/* LVGL rendering — runs on the display work queue                    */
/* ------------------------------------------------------------------ */

static void set_lux(struct zmk_widget_als *widget, struct als_state state)
{
    lv_label_set_text_fmt(widget->obj, "%u", state.lux);
}

static void als_update_cb(struct als_state state)
{
    struct zmk_widget_als *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_lux(widget, state);
    }
}

/* ------------------------------------------------------------------ */
/* State management — shared between ALS callback and display queue   */
/* ------------------------------------------------------------------ */

K_MUTEX_DEFINE(als_state_mutex);
static struct als_state current_als_state;

static void als_work_cb(struct k_work *work)
{
    k_mutex_lock(&als_state_mutex, K_FOREVER);
    struct als_state state = current_als_state;
    k_mutex_unlock(&als_state_mutex);
    als_update_cb(state);
}

K_WORK_DEFINE(als_work, als_work_cb);

/* ------------------------------------------------------------------ */
/* ZMK ambient light callback — called from the ALS polling work queue */
/* ------------------------------------------------------------------ */

static void als_lux_changed(uint32_t lux)
{
    if (!zmk_display_is_initialized()) {
        return;
    }

    k_mutex_lock(&als_state_mutex, K_FOREVER);
    current_als_state.lux = lux;
    k_mutex_unlock(&als_state_mutex);

    k_work_submit_to_queue(zmk_display_work_q(), &als_work);
}

/* ------------------------------------------------------------------ */
/* Widget init                                                         */
/* ------------------------------------------------------------------ */

int zmk_widget_als_init(struct zmk_widget_als *widget, lv_obj_t *parent)
{
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, 120, 30);
    lv_obj_set_style_text_font(widget->obj, &PixelOperatorMono32, 0);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(widget->obj, "-- lx");
    sys_slist_append(&widgets, &widget->node);

    /* Register with the ZMK ambient light service */
    int rc = zmk_ambient_light_register_cb(als_lux_changed);
    if (rc != 0) {
        LOG_WRN("Failed to register ALS widget callback: %d", rc);
    }

    /* Populate with whatever reading we already have (may be 0) */
    k_mutex_lock(&als_state_mutex, K_FOREVER);
    current_als_state.lux = zmk_ambient_light_get_lux();
    k_mutex_unlock(&als_state_mutex);
    als_update_cb(current_als_state);

    return 0;
}

lv_obj_t *zmk_widget_als_obj(struct zmk_widget_als *widget)
{
    return widget->obj;
}
