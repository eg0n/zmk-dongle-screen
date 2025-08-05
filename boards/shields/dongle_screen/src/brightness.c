#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <math.h>

#include <stdlib.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS <= 0
#error "DONGLE_SCREEN_MAX_BRIGHTNESS must be greater than 0!"
#endif

#define BRIGHTNESS_DELAY_MS 2
#define BRIGHTNESS_FADE_DURATION_MS 500
#define AMBIENT_SENSOR_EVALUATION_PERIOD_MS 1000
#define SCREEN_IDLE_TIMEOUT_MS (CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S * 1000)
#define M_PI 3.14159265358979323846

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

static int64_t last_activity = 0;
static uint8_t max_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS;
static uint8_t current_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS;
static uint8_t set_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS;

static uint8_t clamp_brightness(int8_t value) {
  if (value > max_brightness)
    return max_brightness;
  if (value < 0)
    return 0;
  return (uint8_t)value;
}

static void set_screen_brightness(uint8_t to) {
  if (current_brightness == to)
      return;
  LOG_DBG("set_screen_brightness: %d -> %d", current_brightness, to);
  const int step_delay = BRIGHTNESS_DELAY_MS;
  const int abs_diff = abs(to - current_brightness);
  /*
  Adjust the duration of the fade depending on how small the change is:
      - Small changes -> longer fade
      - For larger changes -> cap to avoid long transitions
  */
  int dynamic_duration = abs_diff < 4 ? 1000 : BRIGHTNESS_FADE_DURATION_MS;

  // Clamp duration to always be between 500ms and 1000ms
  if (dynamic_duration < 500)
    dynamic_duration = 500; // Minimum fade duration
  if (dynamic_duration > 1000)
    dynamic_duration = 1000; // Maximum fade duration

  const int steps =
      dynamic_duration / step_delay; // Total number of animation steps

  float diff = to - current_brightness;
  float tmp_brightness = 0.0f;
  uint8_t last_applied =
      255; // Keeps track of last value sent to avoid redundant updates. 225,
           // first "rounded != last_applied" will always true

  for (int i = 0; i <= steps; i++) {
    float t =
        (float)i / steps; // Normalized time value: 0.0 at start, 1.0 at end

    /*
     Cosine easing (ease-in-out): (1 - cos(t * π)) / 2
        - Starts slow, accelerates in the middle, slows down again
        - Produces a smooth S-curve transition flat > steep > flat
    */
    float eased = (1.0f - cosf(t * M_PI)) / 2.0f;
    tmp_brightness =
        current_brightness + diff * eased; // Interpolate the brightness using eased values
    uint8_t rounded =
        (uint8_t)(tmp_brightness +
                  0.5f); // Convert float brightness to nearest integer

    // Only apply brightness if it actually changed - avoids redundant LED
    // updates
    if (rounded != last_applied) {
      LOG_DBG("Applying brightness %d", rounded);
      led_set_brightness(pwm_leds_dev, DISP_BL, rounded);
      last_applied = rounded;
    }
    k_msleep(step_delay);
  }
  // Ensure the final brightness is applied to the end value
  led_set_brightness(pwm_leds_dev, DISP_BL, to);
  current_brightness = to;
}

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0 || \
    CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
/* screen on / off logic*/
typedef enum {
  OFF = 0,
  ON = 1,
  SLEEP = 2,
} screen_state_t;

static screen_state_t screen_state = ON;
static void screen_set(screen_state_t new_state) {
  if (new_state == ON && screen_state != ON) {
    led_on(pwm_leds_dev, DISP_BL);
    set_screen_brightness(clamp_brightness(current_brightness));
    LOG_INF("Screen on (smooth)");
  } else if (screen_state == ON && (new_state == OFF || new_state == SLEEP)) {
    set_screen_brightness(clamp_brightness(current_brightness));
    led_off(pwm_leds_dev, DISP_BL);
    LOG_INF("Screen off (smooth)");
  }
  screen_state = new_state;
}
#endif


#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
/* set up an idle thread to keep track of inactivity*/
void screen_idle_thread(void) {
  while (1) {
    if (screen_state == ON) {
      int64_t now = k_uptime_get();
      int64_t elapsed = now - last_activity;
      int64_t remaining = SCREEN_IDLE_TIMEOUT_MS - elapsed;

      if (remaining <= 0) {
        screen_set(SLEEP);
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

#endif // CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0 || \
    CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
/* respond to keypresses */
static int key_listener(const zmk_event_t *eh) {
  const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev && ev->state) { // Only on key down
    LOG_DBG("Key pressed: keycode=%d", ev->keycode);

#if CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
    if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE) {
      LOG_INF("Brightness UP key recognized!");
      set_brightness = clamp_brightness(set_brightness + CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP);
      set_screen_brightness(set_brightness);
    } else if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE) {
      set_brightness = clamp_brightness(set_brightness - CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP);
      set_screen_brightness(set_brightness);
    } else if (ev->keycode == CONFIG_DONGLE_SCREEN_BACKLIGHT_TOGGLE_KEYCODE) {
      LOG_INF("Backlight TOGGLE key recognized!");
      screen_set(screen_state == ON ? OFF : ON);
    }
#endif
  }
  last_activity = k_uptime_get();
  if (screen_state == SLEEP) {
    screen_set(ON);
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
    k_wakeup(screen_idle_tid);
#endif
  }
  return 0;
}

ZMK_LISTENER(screen_idle, key_listener);
#endif

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
    if (!sensor_sample_fetch(ambient_sensor) &&
        !sensor_channel_get(ambient_sensor, SENSOR_CHAN_LIGHT, &val)) {
      float factor = ambient_deflection_factor(val.val1);
      int8_t deflection = factor * CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP;
      uint8_t new_brightness =
          clamp_brightness(set_brightness + deflection);
      LOG_DBG("Sensor reading: %d, brightness %d/%d -> %d, deflection %d", val.val1, set_brightness, current_brightness, new_brightness, deflection);
      if (screen_state == ON)
        set_screen_brightness(new_brightness);
      else {
        /* screen is off, update state to use when it's on again */
        current_brightness = new_brightness;
      }
    }
    k_sleep(K_MSEC(AMBIENT_SENSOR_EVALUATION_PERIOD_MS));
  }
}

K_THREAD_DEFINE(ambient_light_tid, 512, ambient_light_thread, NULL, NULL, NULL,
                7, 0, 0);

#endif // CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT

static int init_max_brightness(void) {
  set_screen_brightness(max_brightness);
  last_activity = k_uptime_get();
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
  /* wake up the idle thread */
  k_wakeup(screen_idle_tid);
#else
  LOG_INF("Screen idle timeout disabled");
#endif
  return 0;
}

SYS_INIT(init_max_brightness, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);