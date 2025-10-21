/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <fonts.h>
#include <material_32.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/central.h>
#include <zmk/usb.h>

#include "battery.h"

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#define N_BATTERIES (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + 1)
#define SOURCE_OFFSET 1
#else
#define N_BATTERIES ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT
#define SOURCE_OFFSET 0
#endif

#define GRID_CELL_HEIGHT 30
#define GRID_CELL_WIDTH 45

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
typedef struct {
    uint8_t level;
    bool usb_present;
} battery_state_t;
static battery_state_t battery_states[N_BATTERIES];

static void set_battery_symbol(struct zmk_widget_battery *widget) {
    for (uint8_t i = 0; i < lv_obj_get_child_cnt(widget->obj); i++) {
        uint8_t row = i / 2;
        uint8_t col = i % 2;
        uint8_t battery_no = row + SOURCE_OFFSET;

        lv_obj_t *child = lv_obj_get_child(widget->obj, i);
        battery_state_t *state = &battery_states[battery_no];
        LOG_DBG("source: %d, level: %d, usb: %d", battery_no, state->level,
                state->usb_present);

        if (col == 0) {
            if (state->usb_present)
                lv_label_set_text(child, BATTERY_ANDROID_FRAME_BOLT);
            else {
                if (state->level == 0)
                    lv_label_set_text(
                        child, "#202020 " BATTERY_ANDROID_FRAME_QUESTION "#");
                else if (state->level < 13)
                    lv_label_set_text(child, "#cc0080 " BATTERY_ANDROID_0 "#");
                else if (state->level < 25)
                    lv_label_set_text(child,
                                      "#cc0080 " BATTERY_ANDROID_FRAME_1 "#");
                else if (state->level < 38)
                    lv_label_set_text(child,
                                      "#cccc80 " BATTERY_ANDROID_FRAME_2 "#");
                else if (state->level < 40)
                    lv_label_set_text(child,
                                      "#cccc80 " BATTERY_ANDROID_FRAME_3 "#");
                else if (state->level < 63)
                    lv_label_set_text(child,
                                      "#00cc80 " BATTERY_ANDROID_FRAME_4 "#");
                else if (state->level < 75)
                    lv_label_set_text(child,
                                      "#00cc80 " BATTERY_ANDROID_FRAME_5 "#");
                else if (state->level < 88)
                    lv_label_set_text(child,
                                      "#00cc80 " BATTERY_ANDROID_FRAME_6 "#");
                else
                    lv_label_set_text(
                        child, "#00cc80 " BATTERY_ANDROID_FRAME_FULL "#");
            }
        } else if (col == 1) {
            lv_label_set_text_fmt(child, "%i", state->level);
        }
    }
}

void battery_update_cb(battery_state_t *unused) {
    struct zmk_widget_battery *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_symbol(widget);
    }
}

void peripheral_battery_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    uint8_t source = ev->source + SOURCE_OFFSET;
    battery_states[source] =
        (battery_state_t){.level = ev->state_of_charge, .usb_present = false};
}

void central_battery_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev =
        as_zmk_battery_state_changed(eh);
    battery_states[0] = (battery_state_t) {
        .level =
            (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

battery_state_t *battery_get_state(const zmk_event_t *eh) {
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL)
        peripheral_battery_get_state(eh);
    else
        central_battery_get_state(eh);
    return battery_states;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery, battery_state_t *,
                            battery_update_cb, battery_get_state)

ZMK_SUBSCRIPTION(widget_battery, zmk_peripheral_battery_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

ZMK_SUBSCRIPTION(widget_battery, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) ||                                     \
          IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) */

int zmk_widget_battery_init(struct zmk_widget_battery *widget,
                            lv_obj_t *parent) {

    static lv_coord_t col_dsc[] = {GRID_CELL_WIDTH, GRID_CELL_WIDTH,
                                   LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[N_BATTERIES + 1];
    for (uint8_t i = 0; i <= N_BATTERIES; i++)
        row_dsc[i] = GRID_CELL_HEIGHT;
    row_dsc[N_BATTERIES] = LV_GRID_TEMPLATE_LAST;

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);
    lv_obj_set_size(cont, 2 * GRID_CELL_WIDTH, N_BATTERIES * GRID_CELL_HEIGHT);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    widget->obj = cont;

    for (uint8_t i = 0; i < 2 * N_BATTERIES; i++) {
        uint8_t row = i / 2;
        uint8_t col = i % 2;
        lv_obj_t *label = lv_label_create(cont);
        lv_obj_set_grid_cell(label, LV_GRID_ALIGN_STRETCH, col, 1,
                             LV_GRID_ALIGN_STRETCH, row, 1);
        lv_obj_set_style_text_font(label, &PixelOperatorMono32, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(label, "");
        if (col == 0) {
            lv_label_set_recolor(label, true);
            // https://github.com/lvgl/lv_font_conv/issues/132
            lv_obj_set_style_translate_y(label, 8, 0);
        }
    }

    sys_slist_append(&widgets, &widget->node);
    widget_battery_init();
    return 0;
}

lv_obj_t *zmk_widget_battery_obj(struct zmk_widget_battery *widget) {
    return widget->obj;
}
