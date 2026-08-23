#include "animation.h"
#include <zephyr/kernel.h>

LV_IMG_DECLARE(crystal_01);
LV_IMG_DECLARE(crystal_02);
LV_IMG_DECLARE(crystal_03);
LV_IMG_DECLARE(crystal_04);
LV_IMG_DECLARE(crystal_05);
LV_IMG_DECLARE(crystal_06);
LV_IMG_DECLARE(crystal_07);
LV_IMG_DECLARE(crystal_08);
LV_IMG_DECLARE(crystal_09);
LV_IMG_DECLARE(crystal_10);
LV_IMG_DECLARE(crystal_11);
LV_IMG_DECLARE(crystal_12);
LV_IMG_DECLARE(crystal_13);
LV_IMG_DECLARE(crystal_14);
LV_IMG_DECLARE(crystal_15);
LV_IMG_DECLARE(crystal_16);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

const lv_image_dsc_t *anim_imgs[] = {
    &crystal_01, &crystal_02, &crystal_03, &crystal_04,
    &crystal_05, &crystal_06, &crystal_07, &crystal_08,
    &crystal_09, &crystal_10, &crystal_11, &crystal_12,
    &crystal_13, &crystal_14, &crystal_15, &crystal_16,
};

void draw_animation(lv_obj_t *canvas) {
    lv_obj_t *art = lv_animimg_create(canvas);
    lv_obj_center(art);

    lv_animimg_set_src(art, (const void **)anim_imgs, 16);
    lv_animimg_set_duration(art, CONFIG_DONGLE_SCREEN_ANIMATION_MS);
    lv_animimg_set_repeat_count(art, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(art);

    lv_obj_align(art, LV_ALIGN_CENTER, 0, 0);
}

int zmk_widget_animation_init(struct zmk_widget_animation *widget,
                              lv_obj_t *parent) {
    lv_obj_t *canvas;
    widget->obj = canvas = lv_canvas_create(parent);
    lv_obj_set_size(canvas, BUFFER_SIZE, BUFFER_SIZE);
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, BUFFER_SIZE, BUFFER_SIZE,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_style_translate_x(canvas, 0, 0);
    lv_obj_set_style_translate_y(canvas, -15, 0);
    draw_animation(canvas);
    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_animation_obj(struct zmk_widget_animation *widget) {
    return widget->obj;
}