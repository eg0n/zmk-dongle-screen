/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>

#include "wpm.h"
#include <fonts.h>
#include <material_32.h>

#define GRID_CELL_HEIGHT 30
#define GRID_CELL_WIDTH 45

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct wpm_state {
    int wpm;
};

static struct wpm_state get_state(const zmk_event_t *_eh) {
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(_eh);

    return (struct wpm_state){.wpm = ev ? ev->state : 0};
}

static void set_wpm(struct zmk_widget_wpm *widget,
                    struct wpm_state state) {
    lv_obj_t *child = lv_obj_get_child(widget->obj, 1);
    lv_label_set_text_fmt(child, "%i", state.wpm);
}

static void wpm_update_cb(struct wpm_state state) {
    struct zmk_widget_wpm *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_wpm(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm, struct wpm_state,
                            wpm_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_wpm, zmk_wpm_state_changed);

int zmk_widget_wpm_init(struct zmk_widget_wpm *widget,
                               lv_obj_t *parent) {
    static lv_coord_t col_dsc[] = {GRID_CELL_WIDTH, GRID_CELL_WIDTH,
                                   LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {GRID_CELL_HEIGHT, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);
    lv_obj_set_size(cont, 2 * GRID_CELL_WIDTH, GRID_CELL_HEIGHT);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    widget->obj = cont;

    for (uint8_t i = 0; i < 2; i++) {
        lv_obj_t *label = lv_label_create(cont);
        lv_obj_set_grid_cell(label, LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_set_style_text_font(label, &PixelOperatorMono32, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_text(label, "");
        if (i == 0) {
            // https://github.com/lvgl/lv_font_conv/issues/132
            lv_label_set_text(label, KEYBOARD);
            lv_obj_set_style_translate_y(label, 8, 0);
        }
    }
    sys_slist_append(&widgets, &widget->node);
    widget_wpm_init();
    return 0;
}

lv_obj_t *zmk_widget_wpm_obj(struct zmk_widget_wpm *widget) {
    return widget->obj;
}
