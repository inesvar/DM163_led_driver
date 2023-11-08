#include <inttypes.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#include "handle_data.h"

/*
 * Getting the accelerometer from the device tree
 */
#define ACCELEROMETER_NODE DT_ALIAS(accel0)

struct i2c_dt_spec accelerometer_i2c = I2C_DT_SPEC_GET(ACCELEROMETER_NODE);

const struct gpio_dt_spec accelerometer_irq_gpio =
    GPIO_DT_SPEC_GET(ACCELEROMETER_NODE, irq_gpios);

/*
 * Sensor interrupt callback data
 */
static struct gpio_callback sensor_isr_data;

/*
 * Initializing a workqueue thread to handle new data
 */
#define MY_STACK_SIZE 512
#define MY_PRIORITY 13

K_THREAD_STACK_DEFINE(handle_data_stack_area, MY_STACK_SIZE);

static struct k_work_q handle_data_workq;

/*
 * Workqueue job to handle the new data
 */
static struct k_work handle_data_job;

static void sensor_isr(const struct device *dev, struct gpio_callback *cb,
                       uint32_t pins);
static void configure_accelerometer();
static int setup_gpio_irq_and_workqueues();

LOG_MODULE_REGISTER(accelerometer, LOG_LEVEL_INF);

int main(void) {
  if (!i2c_is_ready_dt(&accelerometer_i2c)) {
    LOG_ERR("Accelerometer isn't ready\n");
    return 1;
  }

  if (!setup_gpio_irq_and_workqueues()) {
    return 1;
  }

  configure_accelerometer();

  return 0;
}

/*
 * Accelerometer Interrupt Handler
 * Submit a handle new data job to the workqueue
 */
static void sensor_isr(const struct device *dev, struct gpio_callback *cb,
                       uint32_t pins) {
  k_work_submit_to_queue(&handle_data_workq, &handle_data_job);
}

static void configure_accelerometer() {
  // reset the accelerometer device
  // set bit 0 of register 12 (CTRL3_C) to 1
  uint8_t bit_mask = 1;
  uint8_t value = 1;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x12, bit_mask, value);

  // disable high performance mode for the accelerometer
  // set bit 4 of register 15 (CTRL6_C) to 1
  bit_mask = 1 << 4;
  value = 1 << 4;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x15, bit_mask, value);

  // set output data rate of accelerometer at ACCELEROMETER_ODR Hz
  // set bits [7:4] of register 10 (CTRL1_XL) to 0011
  bit_mask = 0xF << 4;
  value = 0x3 << 4;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x10, bit_mask, value);

  // disable high performance mode for the gyroscope
  // set bit 7 of register 16 (CTRL7_G) to 1
  bit_mask = 1 << 7;
  value = 1 << 7;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x16, bit_mask, value);

  // set output data rate of gyroscope at GYROSCOPE_ODR Hz
  // set bits [7:4] of register 11 (CTRL2_G) to 1000
  bit_mask = 0xF << 4;
  value = 0x8 << 4;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x11, bit_mask, value);

  // allowing the interruption Accelerometer Data Ready on INT1
  // set bit 0 of register 0D (INT1_CTRL) to 1
  bit_mask = 0x3;
  value = 0x3;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x0D, bit_mask, value);
}

static int setup_gpio_irq_and_workqueues() {
  /*
   * Checking that the GPIO is ready
   * configuring it as an input with an interrupt
   */
  if (!device_is_ready(accelerometer_irq_gpio.port)) {
    LOG_ERR("GPIO is not ready\n");
    return 0;
  }

  if (gpio_pin_configure_dt(&accelerometer_irq_gpio, GPIO_INPUT)) {
    LOG_ERR("GPIO configured as input\n");
    return 0;
  }

  if (gpio_pin_interrupt_configure_dt(&accelerometer_irq_gpio,
                                      GPIO_INT_EDGE_RISING)) {
    LOG_ERR("GPIO interruption configured\n");
    return 0;
  }

  // Adding a callback to select the interrupt handler
  gpio_init_callback(&sensor_isr_data, sensor_isr,
                     BIT(accelerometer_irq_gpio.pin));

  if (gpio_add_callback(accelerometer_irq_gpio.port, &sensor_isr_data)) {
    LOG_ERR("Callback couldn't be added\n");
    return 0;
  }

  /*
   * Initializing a workqueue job to execute the blocking I2C
   * operation to handle the new data
   */
  k_work_init(&handle_data_job, handle_new_data);

  k_work_queue_init(&handle_data_workq);

  k_work_queue_start(&handle_data_workq, handle_data_stack_area,
                     K_THREAD_STACK_SIZEOF(handle_data_stack_area), MY_PRIORITY,
                     NULL);

  init_filter_workq();

  LOG_INF("Irq & workqueues setup was successful\n");
  return 1;
}
