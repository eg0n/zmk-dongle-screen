#include <math.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>

#include <stdlib.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS <= 0
#error "DONGLE_SCREEN_MAX_BRIGHTNESS must be greater than 0!"
#endif

#define AMBIENT_SENSOR_EVALUATION_PERIOD_MS 1000
#define SCREEN_IDLE_TIMEOUT_MS (CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S * 1000)
#define M_PI 3.14159265358979323846

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))
#define CLAMP_BL(x) CLAMP((x), 1, CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS)

static int64_t last_activity = 0;
static int8_t ambient_deflection = 0;
static uint8_t set_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS >> 1;
static uint8_t current_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS >> 1;

typedef enum {
  SCREEN_OFF = 0,
  SCREEN_ON = 1,
  SCREEN_SLEEP = 2,
} screen_state_t;

static screen_state_t screen_state = SCREEN_ON;

static void screen_set(screen_state_t new_state) {
  if (new_state == SCREEN_ON && screen_state != SCREEN_ON) {
    led_on(pwm_leds_dev, DISP_BL);
    LOG_INF("Screen on");
  } else if (screen_state == SCREEN_ON && new_state != SCREEN_ON) {
    led_off(pwm_leds_dev, DISP_BL);
    LOG_INF("Screen off");
  }
  screen_state = new_state;
}

static void set_screen_brightness() {
  uint8_t to = CLAMP_BL(set_brightness + ambient_deflection);
  LOG_DBG("set_screen_brightness: %d/%d; %d -> %d", set_brightness, ambient_deflection, current_brightness, to);
  if (screen_state == SCREEN_ON)
    led_set_brightness(pwm_leds_dev, DISP_BL, to);
  current_brightness = to;
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
        screen_set(SCREEN_SLEEP);
        // After turning off, sleep until next activity (key event will wake
        // screen)
        k_sleep(K_FOREVER);
      } else {
        // Sleep exactly as long as needed until timeout or next key event
        k_sleep(K_MSEC(remaining));
      }
    } else {
      // If Screen is off, sleep forever (will be interrupted by key event)
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
  const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev && ev->state) { // Only on key down
    LOG_DBG("Key pressed: keycode=%d", ev->keycode);

#if CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
    if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE) {
      LOG_INF("Brightness UP key recognized!");
      set_brightness =
          CLAMP_BL(set_brightness + CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP);
    } else if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE) {
      set_brightness =
          CLAMP_BL(set_brightness - CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP);
    } else if (ev->keycode == CONFIG_DONGLE_SCREEN_BACKLIGHT_TOGGLE_KEYCODE) {
      LOG_INF("Backlight TOGGLE key recognized!");
      screen_set(screen_state == SCREEN_ON ? SCREEN_OFF : SCREEN_ON);
    }
#endif // CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
    last_activity = k_uptime_get();
    if (screen_state == SCREEN_SLEEP) {
      /* fake out brightness to force an update */
      current_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS;
      screen_set(SCREEN_ON);
    }
    if (screen_state != SCREEN_OFF)
      set_screen_brightness();
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
    k_wakeup(screen_idle_tid);
#endif // CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S
  }
  return 0;
}

ZMK_LISTENER(screen_idle, key_listener);
#endif // IDLE_TIMEOUT || KEYBOARD_CONTROL

ZMK_SUBSCRIPTION(screen_idle, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(screen_idle, zmk_layer_state_changed);

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)
#define AMBIENT_LIGHT_SENSOR_NODE DT_INST(0, avago_apds9960)
static const struct device *ambient_sensor =
    DEVICE_DT_GET(AMBIENT_LIGHT_SENSOR_NODE);
const uint16_t max_sensor = CONFIG_DONGLE_SCREEN_AMBIENT_SENSOR_MAX_VALUE;

static float ambient_deflection_factor(uint16_t sensor_value) {
  /* returns a float from -0.5 to 0.5*/
  if (sensor_value > max_sensor)
    sensor_value = max_sensor;
  return (float)sensor_value / max_sensor - 0.5;
}

static void ambient_light_thread(void) {
  struct sensor_value val;
  while (1) {
    if (!device_is_ready(ambient_sensor)) {
      LOG_ERR("Ambient light sensor not ready!");
      k_sleep(K_SECONDS(5));
      continue;
    }
    if (set_brightness != 0 &&
        !sensor_sample_fetch(ambient_sensor) &&
        !sensor_channel_get(ambient_sensor, SENSOR_CHAN_LIGHT, &val)) {
      ambient_deflection = (int8_t)(ambient_deflection_factor(val.val1) * CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS);
      set_screen_brightness();
      LOG_DBG("Sensor reading: %d, deflection %d", val.val1, ambient_deflection);
    }
    k_sleep(K_MSEC(AMBIENT_SENSOR_EVALUATION_PERIOD_MS));
  }
}

K_THREAD_DEFINE(ambient_light_tid, 512, ambient_light_thread, NULL, NULL, NULL,
                7, 0, 0);

#endif // CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT

static int init_dongle_screen(void) {
  set_screen_brightness();
  last_activity = k_uptime_get();
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
  /* wake up the idle thread */
  k_wakeup(screen_idle_tid);
#else
  LOG_INF("Screen idle timeout disabled");
#endif // CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S
  return 0;
}

SYS_INIT(init_dongle_screen, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);