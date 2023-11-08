#include "complementary_filter.h"

#include <inttypes.h>
#include <math.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#include "blink_leds.h"

#define MSGQ_BUFFER_SIZE 1
#define MSGQ_UPDATE_PERIOD K_MSEC(MSGQ_CHECK_PERIOD_MS)

#define LED_THREAD_STACK_SIZE 512
#define LED_THREAD_PRIORITY 15

/*
 * Define a secondary thread.
 */
static struct k_thread led_thread;

/*
 * Define the message queue and its buffer.
 */
struct k_msgq blink_period_msgq;
static uint8_t
    blink_period_msgq_buffer[sizeof(blink_half_period_ms_t) * MSGQ_BUFFER_SIZE];

K_THREAD_STACK_DEFINE(led_thread_stack, LED_THREAD_STACK_SIZE);

static blink_half_period_ms_t get_blink_half_period_from_measure(
    int board_tilt);
static void update_msgq_if_necessary(blink_half_period_ms_t new_blink_period);
static void send_message();

protected_variable tilt_from_acceleration;
protected_variable tilt_change_from_gyroscope;

LOG_MODULE_REGISTER(complementary_filter, LOG_LEVEL_INF);

void init_complementary_filter() {
  k_mutex_init(&tilt_from_acceleration.mutex);
  k_mutex_init(&tilt_change_from_gyroscope.mutex);

  // Initializing the message queue
  k_msgq_init(&blink_period_msgq, blink_period_msgq_buffer,
              sizeof(blink_half_period_ms_t), MSGQ_BUFFER_SIZE);

  // Spawning a thread for the led task
  k_thread_create(&led_thread, led_thread_stack, LED_THREAD_STACK_SIZE,
                  led_main, NULL, NULL, NULL, LED_THREAD_PRIORITY, 0,
                  K_NO_WAIT);
}
void compute_board_attitude_with_filter() {
  static double complementary_coefficient = 0.98;
  static double tilt = 0;

  k_mutex_lock(&tilt_change_from_gyroscope.mutex, K_FOREVER);
  tilt = tilt_change_from_gyroscope.value * (1 - complementary_coefficient);
  k_mutex_unlock(&tilt_change_from_gyroscope.mutex);

  LOG_DBG("after gyro : %g\n", tilt);

  k_mutex_lock(&tilt_from_acceleration.mutex, K_FOREVER);
  tilt += tilt_from_acceleration.value * complementary_coefficient;
  k_mutex_unlock(&tilt_from_acceleration.mutex);

  LOG_DBG("%g\n", tilt * 180 / 3.1415926535);

  blink_half_period_ms_t blink_period =
      get_blink_half_period_from_measure((int)(tilt * 180 / 3.1415926535));
  update_msgq_if_necessary(blink_period);
}

static blink_half_period_ms_t get_blink_half_period_from_measure(
    int board_tilt) {
  if (board_tilt < 5) {
    return VERY_SLOW_BLINK;
  } else if (board_tilt < 30) {
    return SLOW_BLINK;
  } else if (board_tilt < 55) {
    return REGULAR_BLINK;
  } else if (board_tilt < 80) {
    return FAST_BLINK;
  } else {
    return VERY_FAST_BLINK;
  }
}

static void update_msgq_if_necessary(blink_half_period_ms_t new_blink_period) {
  static blink_half_period_ms_t prev_blink_period = VERY_SLOW_BLINK;
  static blink_half_period_ms_t curr_blink_period;

  curr_blink_period = new_blink_period;
  if (prev_blink_period != curr_blink_period) {
    send_message(&curr_blink_period);
  }
  prev_blink_period = curr_blink_period;
}

static void send_message(blink_half_period_ms_t *blink_period_ptr) {
  // if the message queue is full
  if (k_msgq_num_free_get(&blink_period_msgq) == 0) {
    k_msgq_purge(&blink_period_msgq);
  }
  k_msgq_put(&blink_period_msgq, blink_period_ptr, K_NO_WAIT);
}