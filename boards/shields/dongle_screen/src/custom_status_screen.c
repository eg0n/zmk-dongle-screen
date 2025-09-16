/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"

#if CONFIG_DONGLE_SCREEN_CONNECTION_ACTIVE
#include "widgets/connection.h"
static struct zmk_widget_connection connection_widget;
#endif

#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
#include "widgets/layer.h"
static struct zmk_widget_layer layer_widget;
#endif

#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
#include "widgets/battery.h"
static struct zmk_widget_dongle_battery dongle_battery_widget;
#endif

#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
#include "widgets/wpm.h"
static struct zmk_widget_wpm wpm_widget;
#endif

#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
#include "widgets/modifiers.h"
static struct zmk_widget_modifiers modifiers_widget;
#endif

#if CONFIG_DONGLE_SCREEN_ANIMATION_ACTIVE
#include "widgets/animation.h"
static struct zmk_widget_dongle_animation animation_widget;
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    lv_style_init(&global_style);
    // lv_style_set_text_font(&global_style, &lv_font_unscii_8); // ToDo: Font
    // is not recognized
    lv_style_set_text_color(&global_style, lv_color_white());
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

#if CONFIG_DONGLE_SCREEN_CONNECTION_ACTIVE
    zmk_widget_connection_init(&connection_widget, screen);
    lv_obj_align(zmk_widget_connection_obj(&connection_widget),
                 LV_ALIGN_TOP_RIGHT, -20, 20);
#endif

#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
    zmk_widget_dongle_battery_init(&dongle_battery_widget,
                                          screen);
    lv_obj_align(
        zmk_widget_dongle_battery_obj(&dongle_battery_widget),
        LV_ALIGN_BOTTOM_LEFT, 20, -20);
#endif

#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
    zmk_widget_wpm_init(&wpm_widget, screen);
    lv_obj_align(zmk_widget_wpm_obj(&wpm_widget),
                 LV_ALIGN_TOP_LEFT, 20, 20);
#endif

#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
    zmk_widget_layer_init(&layer_widget, screen);
    lv_obj_align(zmk_widget_layer_obj(&layer_widget),
                 LV_ALIGN_BOTTOM_RIGHT, -20, -20);
#endif

#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget),
                 LV_ALIGN_TOP_MID, 0, 20);
#endif

#if CONFIG_DONGLE_SCREEN_ANIMATION_ACTIVE
    zmk_widget_dongle_animation_init(&animation_widget, screen);
    lv_obj_align(zmk_widget_dongle_animation_obj(&animation_widget),
                 LV_ALIGN_CENTER, 0, 0);
#endif

    return screen;
}
