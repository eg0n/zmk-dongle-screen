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

#include "battery_status.h"

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#define SOURCE_OFFSET 1
#else
#define SOURCE_OFFSET 0
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
typedef struct {
    uint8_t level;
    bool usb_present;
} battery_state_t;
static battery_state_t
    battery_states[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];

static void set_battery_symbol(lv_obj_t *label) {
    char text[128];
    size_t textpos = 0;
    if (SOURCE_OFFSET == 0)
        text[textpos++] = '\n';
    for (int i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET;
         i++) {
        battery_state_t *state = &battery_states[i];
        char *offset = text + textpos;
        LOG_DBG("source: %d, level: %d, usb: %d", i, state->level,
                state->usb_present);
        if (state->usb_present)
            textpos += sprintf(offset, BATTERY_ANDROID_FRAME_BOLT " %i\n",
                               state->level);
        else {
            if (state->level == 0)
                textpos += sprintf(
                    offset, "#ff0000 " BATTERY_ANDROID_FRAME_QUESTION "# X\n");
            else if (state->level < 17)
                textpos +=
                    sprintf(offset, "#ff0000 " BATTERY_ANDROID_FRAME_1 "# %i\n",
                            state->level);
            else if (state->level < 33)
                textpos +=
                    sprintf(offset, "#ffff00 " BATTERY_ANDROID_FRAME_2 "# %i\n",
                            state->level);
            else if (state->level < 50)
                textpos +=
                    sprintf(offset, "#ffff00 " BATTERY_ANDROID_FRAME_3 "# %i\n",
                            state->level);
            else if (state->level < 67)
                textpos +=
                    sprintf(offset, "#00ff00 " BATTERY_ANDROID_FRAME_4 "# %i\n",
                            state->level);
            else if (state->level < 83)
                textpos +=
                    sprintf(offset, "#00ff00 " BATTERY_ANDROID_FRAME_5 "# %i\n",
                            state->level);
            else
                textpos +=
                    sprintf(offset, "#00ff00 " BATTERY_ANDROID_FRAME_6 "# %i\n",
                            state->level);
        }
    }
    lv_label_set_text(label, text);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(label);
}

void battery_status_update_cb(battery_state_t *unused) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_symbol(widget->obj);
    }
}

void peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    uint8_t source = ev->source + SOURCE_OFFSET;
    battery_states[source] =
        (battery_state_t){.level = ev->state_of_charge, .usb_present = false};
}

void central_battery_status_get_state(const zmk_event_t *eh) {
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

battery_state_t *battery_status_get_state(const zmk_event_t *eh) {
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL)
        peripheral_battery_status_get_state(eh);
    else
        central_battery_status_get_state(eh);
    return battery_states;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status, battery_state_t *,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_dongle_battery_status,
                 zmk_peripheral_battery_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) ||                                     \
          IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) */

int zmk_widget_dongle_battery_status_init(
    struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, 100, 100);
    lv_label_set_recolor(widget->obj, true);
    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(widget->obj, &PixelOperatorMono32, 0);

    sys_slist_append(&widgets, &widget->node);
    widget_dongle_battery_status_init();
    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(
    struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}
