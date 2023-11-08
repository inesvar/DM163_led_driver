#include "handle_data.h"

#include <inttypes.h>
#include <math.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#include "complementary_filter.h"

/*
 * Using useful device tree structs.
 */
extern struct i2c_dt_spec accelerometer_i2c;
extern const struct gpio_dt_spec accelerometer_irq_gpio;

/*
 * Workqueue job to compute the tilt
 */
static struct k_work compute_tilt_job;

/*
 * Linear acceleration register address and values
 */
static uint8_t acceleration_register_address = 0x28;
static int16_t acceleration_measure[3];
static uint8_t acceleration_register[6];

/*
 * Angular rate register address and values
 */
static uint8_t angular_rate_register_address = 0x22;
static uint8_t angular_rate_register[4];
static int16_t angular_rate_measure[2];

/*
 * Initializing a workqueue thread to compute the tilt
 * using a complementary filter
 */
#define MY_STACK_SIZE 512
#define MY_PRIORITY 13

K_THREAD_STACK_DEFINE(compute_tilt_stack_area, MY_STACK_SIZE);

static struct k_work_q compute_tilt_workq;

void handle_new_data();
void init_filter_workq();
static void compute_board_tilt_from_acceleration();
static void compute_board_tilt_from_angular_rate();

LOG_MODULE_REGISTER(accelerometer_data, CONFIG_LOG_DEFAULT_LEVEL);

void init_filter_workq() {
  /*
   * Initializing a workqueue job to compute the tilt
   * using a complementary filter
   */
  init_complementary_filter();

  k_work_init(&compute_tilt_job, compute_board_attitude_with_filter);

  k_work_queue_init(&compute_tilt_workq);

  k_work_queue_start(&compute_tilt_workq, compute_tilt_stack_area,
                     K_THREAD_STACK_SIZEOF(compute_tilt_stack_area),
                     MY_PRIORITY, NULL);
}

void handle_new_data() {
  while (gpio_pin_get_dt(&accelerometer_irq_gpio)) {
    // read the status register 1E (STATUS_REG)
    // bit 0 : Accelerometer New Data
    // bit 1 : Gyroscope New Data
    uint8_t status_reg;
    i2c_reg_read_byte_dt(&accelerometer_i2c, 0x1E, &status_reg);

    if (status_reg & 0x1) {
      compute_board_tilt_from_acceleration();
    }

    if (status_reg & 0x2) {
      compute_board_tilt_from_angular_rate();
    }
    k_work_submit_to_queue(&compute_tilt_workq, &compute_tilt_job);
  }
}

static void compute_board_tilt_from_acceleration() {
  // read the contents of all 6 acceleration registers
  // put their contents in a buffer
  i2c_write_read_dt(&accelerometer_i2c, &acceleration_register_address, 1,
                    acceleration_register, 6);

  // get the acceleration measures from register contents
  for (int i = 0; i < 3; i++) {
    uint16_t acceleration =
        acceleration_register[i * 2] | (acceleration_register[i * 2 + 1] << 8);
    acceleration_measure[i] = (int16_t)acceleration;
  }
  int32_t g_xy_squared = acceleration_measure[0] * acceleration_measure[0] +
                         acceleration_measure[1] * acceleration_measure[1];
  int32_t g_z = acceleration_measure[2];

  double g_xy = sqrt(g_xy_squared);
  // compute the attitude
  double tilt_angle = atan2(g_xy, g_z);

  k_mutex_lock(&tilt_from_acceleration.mutex, K_FOREVER);
  tilt_from_acceleration.value = tilt_angle;
  k_mutex_unlock(&tilt_from_acceleration.mutex);
}

static void compute_board_tilt_from_angular_rate() {
  static double angle[2] = {0, 0};

  // read the contents of the 4 needed angular rate registers
  // put their contents in a buffer
  i2c_write_read_dt(&accelerometer_i2c, &angular_rate_register_address, 1,
                    angular_rate_register, 4);

  // integrate the angular rate
  for (int i = 0; i < 2; i++) {
    uint16_t angular_rate =
        angular_rate_register[i * 2] | (angular_rate_register[i * 2 + 1] << 8);
    angular_rate_measure[i] = (int16_t)angular_rate;

    // the scale of the measure is +/- 250 dps, the frequency is GYROSCOPE_ODR
    // Hz
    angle[i] += (float)(angular_rate_measure[i]) / (1 << 15) * 250 /
                GYROSCOPE_ODR / 180 * 3.1415926535;
  }
  LOG_DBG("(Rate measures %hd %hd)\n", angular_rate_measure[0],
          angular_rate_measure[1]);
  double board_tilt = acos(cos(angle[0]) * cos(angle[1]));

  k_mutex_lock(&tilt_change_from_gyroscope.mutex, K_FOREVER);
  tilt_change_from_gyroscope.value = board_tilt;
  k_mutex_unlock(&tilt_change_from_gyroscope.mutex);
}