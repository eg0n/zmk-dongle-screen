#include "modifiers.h"
#include <fonts.h>
#include <lvgl.h>
#include <material_32.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/hid.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void set_modifiers_label(lv_obj_t *label, zmk_mod_flags_t state) {
    lv_label_set_text_fmt(
        label, "%s%s%s%s",
        state & (MOD_LCTL | MOD_RCTL) ? KEYBOARD_CONTROL_KEY : "",
        state & (MOD_LSFT | MOD_RSFT) ? SHIFT : "",
        state & (MOD_LALT | MOD_RALT) ? KEYBOARD_OPTION_KEY : "",
        state & (MOD_LGUI | MOD_RGUI) ? KEYBOARD_COMMAND_KEY : "");
}

static void modifiers_update_cb(zmk_mod_flags_t state) {
    struct zmk_widget_modifiers *widget;
    LOG_DBG("Modifiers callback: %x", state);
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_modifiers_label(widget->obj, state);
    }
}

static zmk_mod_flags_t modifiers_get_state(const zmk_event_t *eh) {
    zmk_mod_flags_t modifiers = zmk_hid_get_keyboard_report()->body.modifiers;
    LOG_DBG("Modifiers get state: %x", modifiers);
    return modifiers;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifiers, zmk_mod_flags_t,
                            modifiers_update_cb,
                            modifiers_get_state)
ZMK_SUBSCRIPTION(widget_modifiers, zmk_modifiers_state_changed);

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget,
                                     lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, 130, 32);
    lv_obj_set_style_text_font(widget->obj, &material_32, 0);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
    sys_slist_append(&widgets, &widget->node);
    widget_modifiers_init();
    return 0;
}

lv_obj_t *
zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget) {
    return widget->obj;
}
