/*
 * SPDX-License-Identifier: MIT
 *
 * boards/shields/dongle_screen/src/behavior_dongle_screen_bl.c
 *
 * Dongle screen brightness behavior for ZMK.
 *
 * Provides the &ds_bl behavior, which routes DS_BL_INC / DS_BL_DEC /
 * DS_BL_TOG / DS_BL_SET commands into ZMK's backlight API.
 *
 * The display PWM LED is already registered as zmk,backlight = &disp_bl
 * in the shield overlay, so zmk_backlight_*() calls drive it directly.
 * No PWM or LED device handles needed here.
 *
 * Keymap example:
 *   #include <behaviors/dongle_screen_bl.dtsi>
 *   #include <dt-bindings/zmk/dongle_screen_bl.h>
 *
 *   &ds_bl DS_BL_INC    // brightness up one step
 *   &ds_bl DS_BL_DEC    // brightness down one step
 *   &ds_bl DS_BL_TOG    // toggle backlight on/off
 *   &ds_bl DS_BL_SET 75 // set to 75%
 */

#define DT_DRV_COMPAT zmk_behavior_dongle_screen_bl

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/backlight.h>
#include <dt-bindings/zmk/dongle_screen_bl.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

/* Brightness step — reuse ZMK backlight's configured step if available */
#ifdef CONFIG_ZMK_BACKLIGHT_BRT_STEP
#define BRT_STEP CONFIG_ZMK_BACKLIGHT_BRT_STEP
#else
#define BRT_STEP 10
#endif

static int behavior_ds_bl_init(const struct device *dev) { return 0; }

static int behavior_ds_bl_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event)
{
    uint32_t cmd  = binding->param1;
    uint32_t param = binding->param2;
    int rc = 0;

    switch (cmd) {
    case DS_BL_INC_CMD:
        rc = zmk_backlight_set_brt(
            MIN((int)zmk_backlight_get_brt() + BRT_STEP, 100));
        break;
    case DS_BL_DEC_CMD:
        rc = zmk_backlight_set_brt(
            MAX((int)zmk_backlight_get_brt() - BRT_STEP, 0));
        break;
    case DS_BL_TOG_CMD:
        rc = zmk_backlight_toggle();
        break;
    case DS_BL_SET_CMD:
        rc = zmk_backlight_set_brt((uint8_t)MIN(param, 100U));
        break;
    default:
        LOG_WRN("Unknown ds_bl command: %d", cmd);
        return -ENOTSUP;
    }

    if (rc != 0) {
        LOG_WRN("ds_bl cmd %d failed: %d", cmd, rc);
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
