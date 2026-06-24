/*
 * SPDX-License-Identifier: MIT
 *
 * boards/shields/dongle_screen/src/brightness.c
 *
 * Ambient-light → backlight bridge for zmk-dongle-screen.
 *
 * The display PWM LED is driven via ZMK's own backlight subsystem
 * (CONFIG_ZMK_BACKLIGHT=y, zmk,backlight = &disp_bl in the overlay).
 * This means zmk_backlight_set_brt() / zmk_backlight_get_brt() are
 * already the correct API for brightness control, and idle blanking is
 * handled by CONFIG_ZMK_BACKLIGHT_AUTO_OFF_IDLE reacting to the
 * zmk_activity_state_changed event from ZMK core — no custom thread
 * or keycode interception needed here.
 *
 * This file's sole remaining job is: when DONGLE_SCREEN_AMBIENT_LIGHT
 * is enabled, register a callback with the ZMK ambient light service
 * and forward lux readings into zmk_backlight_set_brt().
 */


#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/backlight.h>
#include <zmk/ambient_light.h>
#include <zephyr/sys/util.h>
#include <zephyr/init.h>

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define ALS_LUX_MAX ((uint32_t)CONFIG_DONGLE_SCREEN_ALS_LUX_MAX)

static void als_brightness_cb(uint32_t lux)
{
    /* Only adjust while the backlight is on */
    if (!zmk_backlight_is_on()) {
        return;
    }

    uint8_t brt = zmk_ambient_light_lux_to_brt(
        lux,
        5,
        100,
        ALS_LUX_MAX);

    int rc = zmk_backlight_set_brt(brt);
    if (rc != 0) {
        LOG_WRN("backlight_set_brt(%d) failed: %d", brt, rc);
    }
}

static int dongle_screen_brightness_init(void)
{
    return zmk_ambient_light_register_cb(als_brightness_cb);
}

SYS_INIT(dongle_screen_brightness_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT */
