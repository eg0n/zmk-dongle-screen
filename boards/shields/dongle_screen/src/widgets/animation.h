#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define BUFFER_SIZE 69

#define LVGL_BACKGROUND                                                        \
    IS_ENABLED(CONFIG_SCREEN_ANIMATION_INVERTED) ? lv_color_black()            \
                                                 : lv_color_white()
#define LVGL_FOREGROUND                                                        \
    IS_ENABLED(CONFIG_SCREEN_ANIMATION_INVERTED) ? lv_color_white()            \
                                                 : lv_color_black()

/* LVGL 9 canvas buffers are raw bytes in the canvas' colour format, not an
 * array of lv_color_t (which is 3 bytes wide regardless of colour depth). */
struct zmk_widget_animation {
    sys_snode_t node;
    lv_obj_t *obj;
    uint8_t cbuf[LV_CANVAS_BUF_SIZE(BUFFER_SIZE, BUFFER_SIZE, 16,
                                    LV_DRAW_BUF_STRIDE_ALIGN)]
        __aligned(LV_DRAW_BUF_ALIGN);
};

void draw_animation(lv_obj_t *canvas);

int zmk_widget_animation_init(struct zmk_widget_animation *widget,
                              lv_obj_t *parent);
lv_obj_t *zmk_widget_animation_obj(struct zmk_widget_animation *widget);
