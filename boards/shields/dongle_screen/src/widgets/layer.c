/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <fonts.h>
#include <zmk/display.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "layer.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_state {
    uint8_t index;
    const char *label;
};

static void set_layer_symbol(lv_obj_t *label, struct layer_state state) {
    if (state.label == NULL)
        lv_label_set_text_fmt(label, "%i", state.index);
    else {
        char text[10] = {};
        strncpy(text, state.label, sizeof(text));
        for (uint8_t i = 0; i < strlen(text); i++) {
            text[i] = toupper(text[i]);
        }
        lv_label_set_text(label, text);
    }
}

static void layer_update_cb(struct layer_state state) {
    struct zmk_widget_layer *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_symbol(widget->obj, state);
    }
}

static struct layer_state layer_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_state){.index = index,
                                .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer, struct layer_state, layer_update_cb,
                            layer_get_state)

ZMK_SUBSCRIPTION(widget_layer, zmk_layer_state_changed);

int zmk_widget_layer_init(struct zmk_widget_layer *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, 120, 30);
    lv_obj_set_style_text_font(widget->obj, &PixelOperatorMono32, 0);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
    sys_slist_append(&widgets, &widget->node);
    widget_layer_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_obj(struct zmk_widget_layer *widget) {
    return widget->obj;
}
