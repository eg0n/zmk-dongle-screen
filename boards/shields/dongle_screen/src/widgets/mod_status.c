#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/hid.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/event_manager.h>
#include <lvgl.h>
#include "mod_status.h"
#include <fonts.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void set_modifiers_label(lv_obj_t *label, zmk_mod_flags_t state) {
    char text[32] = "";
    int idx = 0;
    char *syms[4];
    int n = 0;

    if (state & (MOD_LCTL | MOD_RCTL))
        syms[n++] = "󰘴";
    if (state & (MOD_LSFT | MOD_RSFT))
        syms[n++] = "󰘶"; // U+F0636
    if (state & (MOD_LALT | MOD_RALT))
        syms[n++] = "󰘵"; // U+F0635
    if (state & (MOD_LGUI | MOD_RGUI))
        syms[n++] = "󰘳"; // U+F0633

    for (int i = 0; i < n; ++i)
        idx += snprintf(&text[idx], sizeof(text) - idx, "%s", syms[i]);

    lv_label_set_text(label, idx ? text : "");
}

static void modifiers_status_update_cb(zmk_mod_flags_t state) {
    struct zmk_widget_modifiers_status *widget;
    LOG_DBG("Modifiers callback: %x", state);
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_modifiers_label(widget->obj, state);
    }
}

static zmk_mod_flags_t modifiers_status_get_state(const zmk_event_t *eh) {
    zmk_mod_flags_t modifiers = zmk_hid_get_keyboard_report()->body.modifiers;
    LOG_DBG("Modifiers get state: %x", modifiers);
    return modifiers;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifiers_status, zmk_mod_flags_t,
                            modifiers_status_update_cb, modifiers_status_get_state)
ZMK_SUBSCRIPTION(widget_modifiers_status, zmk_modifiers_state_changed);

int zmk_widget_modifiers_status_init(struct zmk_widget_modifiers_status *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, 180, 32);
    lv_obj_align(widget->obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(widget->obj, &symbols_40, 0);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
    sys_slist_append(&widgets, &widget->node);
    widget_modifiers_status_init();
    return 0;
}

lv_obj_t *zmk_widget_modifiers_status_obj(struct zmk_widget_modifiers_status *widget) {
    return widget->obj;
}