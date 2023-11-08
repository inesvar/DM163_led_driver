#include "blink_leds.h"

#include <inttypes.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#define MSGQ_CHECK_PERIOD K_MSEC(MSGQ_CHECK_PERIOD_MS)

extern struct k_msgq blink_period_msgq;

static struct gpio_dt_spec led0 =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static struct gpio_dt_spec led1 =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0});

static void check_msgq_during_half_period();
static int get_message(blink_half_period_ms_t *blink_period_ptr);
static int setup_led_device();

LOG_MODULE_REGISTER(blink_led, LOG_LEVEL_INF);

void led_main() {
  if (!setup_led_device()) return;

  // Blinking the leds and checking the message queue
  while (1) {
    gpio_pin_set_dt(&led0, 1);
    gpio_pin_set_dt(&led1, 1);
    check_msgq_during_half_period();
    gpio_pin_set_dt(&led0, 0);
    gpio_pin_set_dt(&led1, 0);
    check_msgq_during_half_period();
  }
}

/*
 * Check the message queue every MSGQ_CHECK_PERIOD
 * for a total duration of a half period.
 * Update curr_blink_period and break if necessary.
 */
static void check_msgq_during_half_period() {
  static blink_half_period_ms_t curr_blink_period = VERY_SLOW_BLINK;
  static blink_half_period_ms_t new_blink_period;

  new_blink_period = curr_blink_period;

  for (int16_t i = 0; i < curr_blink_period / MSGQ_CHECK_PERIOD_MS; i++) {
    // Check the message queue every MSGQ_CHECK_PERIOD
    k_sleep(MSGQ_CHECK_PERIOD);
    int new_msg = get_message(&new_blink_period);
    if (new_msg && curr_blink_period != new_blink_period) {
      curr_blink_period = new_blink_period;
    }
  }
}

/*
 * Put the content of a message in blink_period_ptr.
 * Return 1 if successful.
 */
static int get_message(blink_half_period_ms_t *blink_period_ptr) {
  return !k_msgq_get(&blink_period_msgq, blink_period_ptr, K_NO_WAIT);
}

/*
 * Setup the led devices
 * Return 1 if successful
 */
static int setup_led_device() {
  if (!gpio_is_ready_dt(&led0)) {
    LOG_ERR("Error : LED device %s is not ready; ignoring it\n",
            led0.port->name);
    return 0;
  } else if (!led0.port) {
    LOG_ERR("Error : LED port not found\n");
    return 0;
  } else {
    int ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
    if (ret != 0) {
      LOG_ERR("Error %d: failed to configure LED device %s pin %d\n", ret,
              led0.port->name, led0.pin);
      return 0;
    } else {
      LOG_INF("Set up LED at %s pin %d\n", led0.port->name, led0.pin);
    }
  }
  if (!gpio_is_ready_dt(&led1)) {
    LOG_ERR("Error : LED device %s is not ready; ignoring it\n",
            led1.port->name);
    return 0;
  } else if (!led1.port) {
    LOG_ERR("Error : LED port not found\n");
    return 0;
  } else {
    int ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT);
    if (ret != 0) {
      LOG_ERR("Error %d: failed to configure LED device %s pin %d\n", ret,
              led1.port->name, led1.pin);
      return 0;
    } else {
      LOG_INF("Set up LED at %s pin %d\n", led1.port->name, led1.pin);
      return 1;
    }
  }
}