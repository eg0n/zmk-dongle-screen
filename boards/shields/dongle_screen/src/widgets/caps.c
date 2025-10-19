#include "caps.h"

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/caps_word_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>

#include <fonts.h>
#include <material_32.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static struct caps_state_t {
    bool caps_word_active, caps_lock_active;
} caps_state = {
    .caps_word_active = false,
    .caps_lock_active = false,
};

static void caps_set_label(lv_obj_t *label) {
    if (caps_state.caps_word_active) {
        lv_obj_set_style_text_color(label, lv_color_hex(0x00ffe5),
                                    LV_PART_MAIN);
    } else if (caps_state.caps_lock_active) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xff00e5),
                                    LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_color(label, lv_color_hex(0x202020),
                                    LV_PART_MAIN);
    }
}

static void caps_update_cb() {
    struct zmk_widget_caps *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        caps_set_label(widget->obj);
    }
}

static struct caps_state_t caps_get_state(const zmk_event_t *eh) {
    const struct zmk_caps_word_state_changed *cw_ev =
        as_zmk_caps_word_state_changed(eh);
    if (cw_ev != NULL) {
        LOG_INF("DISP | Caps Word State Changed: %d", cw_ev->active);
        caps_state.caps_word_active = cw_ev->active;
    }

    const struct zmk_keycode_state_changed *kc_ev =
        as_zmk_keycode_state_changed(eh);
    if (kc_ev != NULL) {
        LOG_INF("DISP | Keycode State Changed: %d", kc_ev->keycode);
        if (kc_ev->state && ZMK_HID_USAGE(kc_ev->usage_page, kc_ev->keycode) == CAPSLOCK)
            caps_state.caps_lock_active = !caps_state.caps_lock_active;
    }

    return caps_state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_caps, struct caps_state_t, caps_update_cb,
                            caps_get_state)
ZMK_SUBSCRIPTION(widget_caps, zmk_caps_word_state_changed);
ZMK_SUBSCRIPTION(widget_caps, zmk_keycode_state_changed);


int zmk_widget_caps_init(struct zmk_widget_caps *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);

    lv_obj_set_style_text_font(widget->obj, &material_32, 0);
    lv_label_set_text(widget->obj, KEYBOARD_CAPSLOCK);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0x030303),
                                LV_PART_MAIN);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);

    sys_slist_append(&widgets, &widget->node);

    widget_caps_init();
    return 0;
}

lv_obj_t *zmk_widget_caps_obj(struct zmk_widget_caps *widget) {
    return widget->obj;
}