#include <math.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

#include <stdlib.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS <= 0
#error "DONGLE_SCREEN_MAX_BRIGHTNESS must be greater than 0!"
#endif

#define AMBIENT_SENSOR_EVALUATION_PERIOD_MS 1000
#define SCREEN_IDLE_TIMEOUT_MS (CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S * 1000)

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))
#define CLAMP_BL(x) CLAMP((x), 1, CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS)

static int64_t last_activity = 0;
static int8_t ambient_deflection = 0;
static uint8_t set_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS >> 1;
static uint8_t saved_brightness = 0;
K_MUTEX_DEFINE(fade_mutex);
K_CONDVAR_DEFINE(fade_cond);

typedef enum {
    SCREEN_OFF = 0,
    SCREEN_ON = 1,
    SCREEN_SLEEP = 2,
} screen_state_t;

static screen_state_t screen_state = SCREEN_ON;

static float fade_function(float t) {
    if (t < 0.5f)
        return 4.0f * pow(t, 3);
    float f = -2.0f * t + 2.0f;
    return 1.0f - pow(f, 3) / 2.0f;
}

void screen_animation_thread(void *, void *, void *) {
    uint8_t current_brightness = set_brightness;
    screen_state_t current_screen_state = SCREEN_ON;
    k_mutex_lock(&fade_mutex, K_FOREVER);
    while (1) {
        k_condvar_wait(&fade_cond, &fade_mutex, K_FOREVER);
        if (current_screen_state != SCREEN_ON && screen_state != SCREEN_ON)
            continue;
        if (current_brightness == 0 && set_brightness > 0)
            led_on(pwm_leds_dev, DISP_BL);
        LOG_DBG("screen animation update: %d -> %d / %d", current_brightness,
                set_brightness, ambient_deflection);
        uint8_t target_brightness =
            set_brightness == 0 ? 0
                                : CLAMP_BL(set_brightness + ambient_deflection);
        int8_t diff = target_brightness - current_brightness;
        /* skip small updates*/
        if (abs(diff) < 2)
            continue;
        /* more steps for smoother fades over large differences */
        uint8_t steps = CLAMP(abs(diff) * 2, 6, 32);
        /* set total animation time: scale with difference but clamp */
        uint8_t duration_ms = CLAMP(abs(diff) * 20, 500, 1000);
        /* delay between steps in microseconds */
        uint16_t delay_us = (duration_ms * 1000) / steps;
        LOG_DBG("screen animation update: duration %d, delay %d, steps %d",
                duration_ms, delay_us, steps);
        uint8_t last_applied = 255;
        /* animate! */
        for (uint8_t i = 0; i <= steps; i++) {
            float t = (float)i / steps;
            float func_val = fade_function(t);
            float interpolated =
                current_brightness +
                (target_brightness - current_brightness) * func_val;
            uint8_t brightness = (uint8_t)(interpolated + 0.5f);
            if (brightness != last_applied) {
                led_set_brightness(pwm_leds_dev, DISP_BL, brightness);
                last_applied = brightness;
            }
            k_usleep(delay_us);
        }
        LOG_DBG("screen animation complete!");
        current_brightness = target_brightness;
        current_screen_state = screen_state;
        if (current_brightness == 0)
            led_off(pwm_leds_dev, DISP_BL);
    }
}

K_THREAD_DEFINE(animation_tid, 768, screen_animation_thread, NULL, NULL, NULL,
                6, 0, 0);

static void update_screen(screen_state_t new_screen_state,
                          uint8_t new_brightness) {
    k_mutex_lock(&fade_mutex, K_FOREVER);
    screen_state = new_screen_state;
    set_brightness = new_brightness;
    k_condvar_signal(&fade_cond);
    k_mutex_unlock(&fade_mutex);
}

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
/* set up an idle thread to keep track of inactivity*/
void screen_idle_thread(void) {
    while (1) {
        if (screen_state == SCREEN_ON) {
            int64_t now = k_uptime_get();
            int64_t elapsed = now - last_activity;
            int64_t remaining = SCREEN_IDLE_TIMEOUT_MS - elapsed;

            if (remaining <= 0) {
                saved_brightness = set_brightness;
                update_screen(SCREEN_SLEEP, 0);
                /* after turning off, sleep until next activity */
                k_sleep(K_FOREVER);
            } else {
                /* sleep exactly as long as needed until timeout or next
                 * activity */
                k_sleep(K_MSEC(remaining));
            }
        } else {
            /* if screen is not on, sleep until next activity*/
            k_sleep(K_FOREVER);
        }
    }
}

K_THREAD_DEFINE(screen_idle_tid, 512, screen_idle_thread, NULL, NULL, NULL, 7,
                0, 0);
#endif // CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0 ||                                 \
    CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
/* respond to keypresses */
static int key_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev =
        as_zmk_keycode_state_changed(eh);
    /* if we're coming out of OFF or SLEEP, recall the saved brightness */
    uint8_t new_brightness =
        screen_state == SCREEN_ON ? set_brightness : saved_brightness;
    screen_state_t new_screen_state =
        screen_state == SCREEN_OFF ? SCREEN_OFF : SCREEN_ON;
    /* only on key down */
    if (ev && ev->state) {
        LOG_DBG("Key pressed: keycode=%d", ev->keycode);
#if CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
        if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE) {
            LOG_INF("Brightness UP key recognized!");
            new_brightness =
                CLAMP_BL(set_brightness + CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP);
        } else if (ev->keycode ==
                   CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE) {
            LOG_INF("Brightness DOWN key recognized!");
            new_brightness =
                CLAMP_BL(set_brightness - CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP);
        } else if (ev->keycode ==
                   CONFIG_DONGLE_SCREEN_BACKLIGHT_TOGGLE_KEYCODE) {
            LOG_INF("Backlight TOGGLE key recognized!");
            if (screen_state == SCREEN_OFF) {
                new_brightness = saved_brightness;
                new_screen_state = SCREEN_ON;
            } else {
                saved_brightness = set_brightness;
                new_brightness = 0;
                new_screen_state = SCREEN_OFF;
            }
        }
#endif // CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
        last_activity = k_uptime_get();
        update_screen(new_screen_state, new_brightness);
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
        k_wakeup(screen_idle_tid);
#endif // CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S
    }
    return 0;
}

ZMK_LISTENER(screen_idle, key_listener);
#endif // IDLE_TIMEOUT || KEYBOARD_CONTROL

ZMK_SUBSCRIPTION(screen_idle, zmk_keycode_state_changed);

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)
#define AMBIENT_LIGHT_SENSOR_NODE DT_INST(0, avago_apds9960)
static const struct device *ambient_sensor =
    DEVICE_DT_GET(AMBIENT_LIGHT_SENSOR_NODE);

static void ambient_light_thread(void) {
    struct sensor_value val, last_val = {.val1 = 255};
    while (1) {
        if (!device_is_ready(ambient_sensor)) {
            LOG_ERR("Ambient light sensor not ready!");
            k_sleep(K_SECONDS(5));
            continue;
        }
        if (!sensor_sample_fetch(ambient_sensor) &&
            !sensor_channel_get(ambient_sensor, SENSOR_CHAN_LIGHT, &val) &&
            val.val1 != last_val.val1) {
            /* Limit the value between 0 and the configured max, then scale that
             * reading to a float between -0.5 and 0.5
             */
            uint8_t sensor_reading = CLAMP(
                val.val1, 0, CONFIG_DONGLE_SCREEN_AMBIENT_SENSOR_MAX_VALUE);
            ambient_deflection =
                (int8_t)(((float)sensor_reading /
                              CONFIG_DONGLE_SCREEN_AMBIENT_SENSOR_MAX_VALUE -
                          0.5) *
                         CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS);
            /* indicate that we should update the brightness */
            update_screen(screen_state, set_brightness);
            last_val = val;
            LOG_DBG("Sensor reading: %d, deflection %d", sensor_reading,
                    ambient_deflection);
        }
        k_sleep(K_MSEC(AMBIENT_SENSOR_EVALUATION_PERIOD_MS));
    }
}

K_THREAD_DEFINE(ambient_light_tid, 512, ambient_light_thread, NULL, NULL, NULL,
                6, 0, 0);

#endif // CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT

static int init_dongle_screen(void) {
    last_activity = k_uptime_get();
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
    /* wake up the idle thread */
    k_wakeup(screen_idle_tid);
#endif // CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S
    return 0;
}

SYS_INIT(init_dongle_screen, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);