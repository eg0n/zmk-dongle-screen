/*
 * SPDX-License-Identifier: MIT
 *
 * boards/shields/dongle_screen/src/brightness.c
 *
 * Ambient-light → backlight bridge for zmk-dongle-screen, with
 * animated brightness transitions.
 *
 * The display PWM LED is driven via ZMK's backlight subsystem
 * (zmk,backlight = &disp_bl in the overlay, ZMK_BACKLIGHT=y in
 * Kconfig.defconfig).  All brightness changes go through
 * zmk_backlight_set_brt() so ZMK's state machine stays consistent.
 *
 * When CONFIG_DONGLE_SCREEN_BRIGHTNESS_FADE=y (default), brightness
 * changes are animated using a cubic ease-in-out curve over a
 * configurable duration.  A dedicated low-priority thread handles the
 * animation loop; a K_MSGQ with depth 1 (purge-on-write) ensures that
 * only the most recent target is ever in flight.
 *
 * The easing is computed in fixed-point integer arithmetic — no float,
 * no math.h required.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/backlight.h>
#include <zephyr/sys/util.h>
#include <zephyr/init.h>

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)
#include <zmk/ambient_light.h>
#endif /* CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT */


LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/* Fixed-point cubic ease-in-out                                       */
/*                                                                    */
/* Input:  t256 ∈ [0, 256]  (normalised time, 256 = 1.0)             */
/* Output: eased ∈ [0, 256]                                           */
/*                                                                    */
/* Cubic ease-in-out:                                                 */
/*   t < 0.5:  f(t) = 4t³                                            */
/*   t ≥ 0.5:  f(t) = 1 - (-2t+2)³/2                                */
/*                                                                    */
/* All intermediate values fit in uint32_t up to t256=256.           */
/* ------------------------------------------------------------------ */
static uint32_t ease_in_out_256(uint32_t t256)
{
    if (t256 >= 256U) return 256U;

    if (t256 < 128U) {
        /* 4t³, scaled: 4 * (t256/256)³ * 256
         * = 4 * t256³ / 256² */
        uint32_t t3 = t256 * t256 * t256;          /* max 128³ = 2 097 152 */
        return (4U * t3) / (256U * 256U);
    } else {
        /* 1 - (-2t+2)³/2
         * Let u = 256 - t256  (u ∈ [0,128] when t256 ∈ [128,256])
         * f = 256 - 4*u³/256² */
        uint32_t u   = 256U - t256;
        uint32_t u3  = u * u * u;                  /* max 128³ = 2 097 152 */
        uint32_t sub = (4U * u3) / (256U * 256U);
        return 256U - sub;
    }
}

/* ------------------------------------------------------------------ */
/* Fade thread                                                         */
/* ------------------------------------------------------------------ */

struct fade_req {
    uint8_t from;
    uint8_t to;
};

/* Depth-1 queue: purge before each put so only the latest target     */
/* is ever queued.  Alignment 4 keeps the struct naturally aligned.   */
K_MSGQ_DEFINE(fade_msgq, sizeof(struct fade_req), 1, 4);

static void fade_thread_fn(void *a, void *b, void *c)
{
    struct fade_req req;

    while (1) {
        k_msgq_get(&fade_msgq, &req, K_FOREVER);

        if (req.from == req.to) {
            zmk_backlight_set_brt(req.to);
            continue;
        }

        int diff  = (int)req.to - (int)req.from;
        int adiff = diff < 0 ? -diff : diff;

        /* Skip animation for very small changes */
        if (adiff <= 1) {
            zmk_backlight_set_brt(req.to);
            continue;
        }

        /* Steps: more steps for larger brightness swings, clamped.
         * Duration: CONFIG_DONGLE_SCREEN_ANIMATION_MS (default 960ms),
         * scaled proportionally for small changes. */
        int steps         = CLAMP(adiff * 2, 8, 32);
        int duration_ms   = CLAMP(
            CONFIG_DONGLE_SCREEN_ANIMATION_MS * adiff / 100,
            CONFIG_DONGLE_SCREEN_ANIMATION_MS / 4,
            CONFIG_DONGLE_SCREEN_ANIMATION_MS);
        int step_delay_us = (duration_ms * 1000) / steps;

        uint8_t last = 255; /* sentinel — forces first apply */

        for (int i = 0; i <= steps; i++) {
            /* Check whether a newer request has arrived mid-animation */
            if (k_msgq_num_used_get(&fade_msgq) > 0) {
                break;
            }

            uint32_t t256   = ((uint32_t)i * 256U) / (uint32_t)steps;
            uint32_t eased  = ease_in_out_256(t256);
            int      interp = (int)req.from + (diff * (int)eased) / 256;
            uint8_t  brt    = (uint8_t)CLAMP(interp, 0, 100);

            if (brt != last) {
                zmk_backlight_set_brt(brt);
                last = brt;
            }

            k_usleep(step_delay_us);
        }

        /* Guarantee the target is reached */
        if (last != req.to) {
            zmk_backlight_set_brt(req.to);
        }
    }
}

/* Stack: 512 bytes — no float, no heavy includes, just integer math  */
/* and a handful of stack locals.  Priority 7 (below key scanning).   */
K_THREAD_DEFINE(ds_fade_tid, 512,
                fade_thread_fn, NULL, NULL, NULL,
                7, 0, 0);

/* ------------------------------------------------------------------ */
/* Public brightness setter — used by ALS callback and behavior driver */
/* ------------------------------------------------------------------ */

void dongle_screen_fade_to_brt(uint8_t target_pct)
{
    struct fade_req req = {
        .from = zmk_backlight_get_brt(),
        .to   = CLAMP(target_pct, 0, 100),
    };
    k_msgq_purge(&fade_msgq);
    k_msgq_put(&fade_msgq, &req, K_NO_WAIT);
}

/* ------------------------------------------------------------------ */
/* Ambient light callback                                              */
/* ------------------------------------------------------------------ */

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)

static void als_brightness_cb(uint32_t lux)
{
    if (!zmk_backlight_is_on()) {
        return;
    }

    uint8_t target = zmk_ambient_light_lux_to_brt(
        lux,
        5U,
        100U,
        (uint32_t)CONFIG_DONGLE_SCREEN_ALS_LUX_MAX);

    dongle_screen_fade_to_brt(target);
}

#endif /* CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT */

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

static int dongle_screen_brightness_init(void)
{
#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)
    int rc = zmk_ambient_light_register_cb(als_brightness_cb);
    if (rc != 0) {
        LOG_WRN("Failed to register ALS brightness callback: %d", rc);
    }
#endif
    return 0;
}

SYS_INIT(dongle_screen_brightness_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
