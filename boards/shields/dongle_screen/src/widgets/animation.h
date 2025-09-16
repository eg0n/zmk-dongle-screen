#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define BUFFER_SIZE 69

#define LVGL_BACKGROUND                                                        \
    IS_ENABLED(CONFIG_DONGLE_SCREEN_ANIMATION_INVERTED) ? lv_color_black()     \
                                                        : lv_color_white()
#define LVGL_FOREGROUND                                                        \
    IS_ENABLED(CONFIG_DONGLE_SCREEN_ANIMATION_INVERTED) ? lv_color_white()     \
                                                        : lv_color_black()

struct zmk_widget_dongle_animation {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_color_t cbuf[BUFFER_SIZE * BUFFER_SIZE];
};

void draw_animation(lv_obj_t *canvas);

int zmk_widget_dongle_animation_init(struct zmk_widget_dongle_animation *widget,
                                     lv_obj_t *parent);
lv_obj_t *
zmk_widget_dongle_animation_obj(struct zmk_widget_dongle_animation *widget);
