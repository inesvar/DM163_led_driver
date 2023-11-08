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

protected_variable tilt_from_acceleration;
protected_variable tilt_change_from_gyroscope;

LOG_MODULE_REGISTER(complementary_filter, LOG_LEVEL_INF);

void init_complementary_filter() {
  k_mutex_init(&tilt_from_acceleration.mutex);
  k_mutex_init(&tilt_change_from_gyroscope.mutex);
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

  LOG_PRINTK("%g\n", tilt * 180 / 3.1415926535);
}