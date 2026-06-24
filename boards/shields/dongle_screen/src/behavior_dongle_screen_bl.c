/*
 * SPDX-License-Identifier: MIT
 *
 * boards/shields/dongle_screen/src/behavior_dongle_screen_bl.c
 *
 * Dongle screen brightness behavior for ZMK.
 *
 * Routes DS_BL_INC / DS_BL_DEC / DS_BL_TOG / DS_BL_SET commands through
 * dongle_screen_fade_to_brt() so key-triggered brightness changes are
 * animated with the same cubic ease-in-out as ALS-driven changes.
 *
 * DS_BL_TOG uses zmk_backlight_toggle() directly — no fade makes sense
 * for an on/off transition that is already handled by ZMK_BACKLIGHT_AUTO_
 * OFF_IDLE.
 *
 * Keymap usage:
 *   #include <behaviors/dongle_screen_bl.dtsi>
 *   #include <dt-bindings/zmk/dongle_screen_bl.h>
 *
 *   &ds_bl DS_BL_INC       — animated brightness up one step
 *   &ds_bl DS_BL_DEC       — animated brightness down one step
 *   &ds_bl DS_BL_TOG       — toggle on/off
 *   &ds_bl DS_BL_SET 75    — animate to 75%
 */

#define DT_DRV_COMPAT zmk_behavior_dongle_screen_bl

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/backlight.h>
#include <dt-bindings/zmk/dongle_screen_bl.h>

#include "brightness.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#ifdef CONFIG_ZMK_BACKLIGHT_BRT_STEP
#define BRT_STEP CONFIG_ZMK_BACKLIGHT_BRT_STEP
#else
#define BRT_STEP 10
#endif

static int behavior_ds_bl_init(const struct device *dev) { return 0; }

static int behavior_ds_bl_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event)
{
    uint32_t cmd   = binding->param1;
    uint32_t param = binding->param2;
    int      rc    = 0;

    switch (cmd) {
    case DS_BL_INC_CMD: {
        int target = (int)zmk_backlight_get_brt() + BRT_STEP;
        dongle_screen_fade_to_brt((uint8_t)MIN(target, 100));
        break;
    }
    case DS_BL_DEC_CMD: {
        int target = (int)zmk_backlight_get_brt() - BRT_STEP;
        dongle_screen_fade_to_brt((uint8_t)MAX(target, 0));
        break;
    }
    case DS_BL_TOG_CMD:
        rc = zmk_backlight_toggle();
        break;
    case DS_BL_SET_CMD:
        dongle_screen_fade_to_brt((uint8_t)MIN(param, 100U));
        break;
    default:
        LOG_WRN("Unknown ds_bl command: %d", cmd);
        return -ENOTSUP;
    }

    return rc;
}

static int behavior_ds_bl_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event)
{
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_ds_bl_driver_api = {
    .binding_pressed  = behavior_ds_bl_pressed,
    .binding_released = behavior_ds_bl_released,
};

BEHAVIOR_DT_INST_DEFINE(0,
    behavior_ds_bl_init, NULL,
    NULL, NULL,
    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
    &behavior_ds_bl_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
