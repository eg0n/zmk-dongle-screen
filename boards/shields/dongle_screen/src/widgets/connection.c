/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#include "connection.h"
#include <fonts.h>
#include <material_32.h>

#define GRID_CELL_HEIGHT 30
#define GRID_CELL_WIDTH 45

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct connection_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool usb_is_hid_ready;
};

static struct connection_state get_state(const zmk_event_t *_eh) {
    return (struct connection_state){
        .selected_endpoint = zmk_endpoints_selected(), // 0 = USB , 1 = BLE
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
        .usb_is_hid_ready = zmk_usb_is_hid_ready()};
}

static void set_symbol(struct zmk_widget_connection *widget,
                              struct connection_state state) {
    for (uint8_t i = 0; i < lv_obj_get_child_cnt(widget->obj); i++) {
        lv_obj_t *child = lv_obj_get_child(widget->obj, i);
        if (i == 1) {
            switch (state.selected_endpoint.transport) {
            case ZMK_TRANSPORT_USB:
                lv_label_set_text(child, "#00ff00 " USB "#");
                break;
            case ZMK_TRANSPORT_BLE:
                if (state.active_profile_bonded) {
                    if (state.active_profile_connected) {
                        lv_label_set_text(child,
                                          "#00ff00 " ANDROID_WIFI_3_BAR "#");
                    } else {
                        lv_label_set_text(
                            child, "#0000ff " ANDROID_WIFI_3_BAR_OFF "#");
                    }
                } else {
                    lv_label_set_text(
                        child, "#ff0000 " ANDROID_WIFI_3_BAR_QUESTION "#");
                }
                break;
            }
        } else if (i == 0) {
            switch (state.selected_endpoint.transport) {
            case ZMK_TRANSPORT_USB:
                lv_label_set_text(child, "");
                break;
            case ZMK_TRANSPORT_BLE:
                lv_label_set_text_fmt(child, "%i", state.active_profile_index);
                break;
            }
        }
    }
}

static void connection_update_cb(struct connection_state state) {
    struct zmk_widget_connection *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_symbol(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_connection, struct connection_state,
                            connection_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_connection, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_connection, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_connection, zmk_usb_conn_state_changed);

int zmk_widget_connection_init(struct zmk_widget_connection *widget,
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
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(label, "");
        if (i == 1) {
            lv_label_set_recolor(label, true);
            // https://github.com/lvgl/lv_font_conv/issues/132
            lv_obj_set_style_translate_y(label, 8, 0);
        }
    }
    sys_slist_append(&widgets, &widget->node);
    widget_connection_init();
    return 0;
}

lv_obj_t *
zmk_widget_connection_obj(struct zmk_widget_connection *widget) {
    return widget->obj;
}
