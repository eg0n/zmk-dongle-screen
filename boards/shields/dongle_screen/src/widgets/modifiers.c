#include "modifiers.h"
#include <fonts.h>
#include <lvgl.h>
#include <material_32.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/caps_word_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/hid.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define GRID_CELL_HEIGHT 30
#define GRID_CELL_WIDTH 30

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static struct mods_state_t {
    bool caps_word_active, caps_lock_active;
    zmk_mod_flags_t mods;
} mods_state = {
    .caps_word_active = false, .caps_lock_active = false, .mods = 0};

static void set_modifiers_label(lv_obj_t *grid, struct mods_state_t state) {
    lv_obj_t *label;
    lv_color_t label_colors[5] = {
        state.mods & (MOD_LSFT | MOD_RSFT) ? lv_color_white()
                                           : lv_color_hex(0x202020),
        state.mods & (MOD_LCTL | MOD_RCTL) ? lv_color_white()
                                           : lv_color_hex(0x202020),
        state.caps_word_active   ? lv_color_hex(0x00cc80)
        : state.caps_lock_active ? lv_color_hex(0xcc0080)
                                 : lv_color_hex(0x202020),
        state.mods & (MOD_LALT | MOD_RALT) ? lv_color_white()
                                           : lv_color_hex(0x202020),
        state.mods & (MOD_LGUI | MOD_RGUI) ? lv_color_white()
                                           : lv_color_hex(0x202020),
    };

    for (uint8_t idx = 0; idx < 5; idx++) {
        label = lv_obj_get_child(grid, idx);
        lv_obj_set_style_text_color(label, label_colors[idx], 0);
    }
}

static void modifiers_update_cb(struct mods_state_t state) {
    struct zmk_widget_modifiers *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_modifiers_label(widget->obj, state);
    }
}

static struct mods_state_t modifiers_get_state(const zmk_event_t *eh) {
    const struct zmk_caps_word_state_changed *cw_ev =
        as_zmk_caps_word_state_changed(eh);
    if (cw_ev != NULL) {
        LOG_INF("Caps Word State Changed: %d", cw_ev->active);
        mods_state.caps_word_active = cw_ev->active;
        return mods_state;
    }

    const struct zmk_keycode_state_changed *kc_ev =
        as_zmk_keycode_state_changed(eh);
    if (kc_ev != NULL) {
        LOG_INF("Keycode State Changed: %d", kc_ev->keycode);
        if (kc_ev->state &&
            ZMK_HID_USAGE(kc_ev->usage_page, kc_ev->keycode) == CAPSLOCK)
            mods_state.caps_lock_active = !mods_state.caps_lock_active;
        return mods_state;
    }

    mods_state.mods = zmk_hid_get_keyboard_report()->body.modifiers;
    return mods_state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifiers, struct mods_state_t,
                            modifiers_update_cb, modifiers_get_state)
ZMK_SUBSCRIPTION(widget_modifiers, zmk_modifiers_state_changed);
ZMK_SUBSCRIPTION(widget_modifiers, zmk_caps_word_state_changed);
ZMK_SUBSCRIPTION(widget_modifiers, zmk_keycode_state_changed);

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget,
                              lv_obj_t *parent) {
    static lv_coord_t col_dsc[] = {GRID_CELL_WIDTH, GRID_CELL_WIDTH,
                                   GRID_CELL_WIDTH, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {GRID_CELL_HEIGHT, GRID_CELL_HEIGHT,
                                   LV_GRID_TEMPLATE_LAST};

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);
    lv_obj_set_size(cont, 3 * GRID_CELL_WIDTH, 2 * GRID_CELL_HEIGHT);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    widget->obj = cont;

    char *label_text[] = {SHIFT, KEYBOARD_CONTROL_KEY, KEYBOARD_CAPSLOCK,
                          KEYBOARD_OPTION_KEY, KEYBOARD_COMMAND_KEY};
    for (uint8_t idx = 0; idx < 5; idx++) {
        lv_obj_t *label = lv_label_create(cont);
        lv_obj_set_style_text_font(label, &material_32, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_grid_cell(label, LV_GRID_ALIGN_STRETCH, idx % 3, 1,
                             LV_GRID_ALIGN_STRETCH, idx / 3, idx == 2 ? 2 : 1);
        lv_label_set_text(label, label_text[idx]);
        lv_obj_set_style_text_color(label, lv_color_hex(0x202020), 0);
    }
    lv_obj_set_style_translate_y(lv_obj_get_child(cont, 2), 10, 0);

    sys_slist_append(&widgets, &widget->node);
    widget_modifiers_init();
    return 0;
}

lv_obj_t *zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget) {
    return widget->obj;
}
