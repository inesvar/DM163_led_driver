#include <inttypes.h>
#include <math.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

/*
 * Getting the accelerometer from the device tree
 */
#define ACCELEROMETER_NODE DT_ALIAS(accel0)

static struct i2c_dt_spec accelerometer_i2c =
    I2C_DT_SPEC_GET(ACCELEROMETER_NODE);

static const struct gpio_dt_spec accelerometer_irq_gpio =
    GPIO_DT_SPEC_GET(ACCELEROMETER_NODE, irq_gpios);

/*
 * Sensor interrupt callback data
 */
static struct gpio_callback sensor_isr_data;

/*
 * Workqueue job to handle the new data
 */
static struct k_work handle_new_data_job;

/*
 * Initializing a workqueue thread to handle new data
 */
#define MY_STACK_SIZE 512
#define MY_PRIORITY 13

K_THREAD_STACK_DEFINE(my_stack_area, MY_STACK_SIZE);

static struct k_work_q my_work_q;

/*
 * Linear acceleration register address and values
 */
static uint8_t acceleration_register_address = 0x28;
static int16_t acceleration_measure[3];
static uint8_t acceleration_register[6];

/*
 * Angular rate register address and values
 */
static char axis_label[3] = {'X', 'Y', 'Z'};
static uint8_t angular_rate_register_address = 0x22;
static uint8_t angular_rate_register[6];
static int16_t angular_rate_measure[3];

static void sensor_isr(const struct device *dev, struct gpio_callback *cb,
                       uint32_t pins);
static void handle_new_data();
static void compute_board_tilt_from_acceleration();
static void print_angular_rate();
static void configure_accelerometer();
static int setup_gpio_irq_and_workqueue();

LOG_MODULE_REGISTER(accelerometer, LOG_LEVEL_INF);

int main(void) {
  if (!i2c_is_ready_dt(&accelerometer_i2c)) {
    LOG_ERR("Accelerometer isn't ready\n");
    return 1;
  }

  if (!setup_gpio_irq_and_workqueue()) {
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
  k_work_submit_to_queue(&my_work_q, &handle_new_data_job);
}

static void handle_new_data() {
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
      print_angular_rate();
    }
    LOG_PRINTK("\n");
  }
}

static void compute_board_tilt_from_acceleration() {
  // read the contents of all 6 acceleration registers
  // put their contents in a buffer
  i2c_write_read_dt(&accelerometer_i2c, &acceleration_register_address, 1,
                    acceleration_register, 6);

  // get the acceleration measures from register contents
  // compute the norm of the acceleration
  int32_t squared_norm = 0;
  for (int i = 0; i < 3; i++) {
    uint16_t acceleration =
        acceleration_register[i * 2] | (acceleration_register[i * 2 + 1] << 8);
    acceleration_measure[i] = (int16_t)acceleration;
    squared_norm += acceleration_measure[i] * acceleration_measure[i];
  }
  double norm = sqrt(squared_norm);
  // compute the tilt angle
  double tilt_angle = acos(acceleration_measure[2] / norm) * 180 / 3.1415926535;
  LOG_PRINTK("tilt angle : %g°\n", tilt_angle);
}

static void print_angular_rate() {
  // read the contents of all 6 angular rate registers
  // put their contents in a buffer
  i2c_write_read_dt(&accelerometer_i2c, &angular_rate_register_address, 1,
                    angular_rate_register, 6);

  // print the angular rate values
  for (int i = 0; i < 3; i++) {
    uint16_t angular_rate =
        angular_rate_register[i * 2] | (angular_rate_register[i * 2 + 1] << 8);
    angular_rate_measure[i] = (int16_t)angular_rate;
  }
  LOG_PRINTK(
      "Angular rate axis %c : %hd\nAngular rate axis %c : %hd\n"
      "Angular rate axis %c : %hd\n",
      axis_label[0], angular_rate_measure[0], axis_label[1],
      angular_rate_measure[1], axis_label[2], angular_rate_measure[2]);
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

  // set output data rate of accelerometer at 52Hz
  // set bits [7:4] of register 10 (CTRL1_XL) to 0011
  bit_mask = 0xF << 4;
  value = 0x3 << 4;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x10, bit_mask, value);

  // disable high performance mode for the gyroscope
  // set bit 7 of register 16 (CTRL7_G) to 1
  bit_mask = 1 << 7;
  value = 1 << 7;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x16, bit_mask, value);

  // set output data rate of gyroscope at 12.5Hz
  // set bits [7:4] of register 11 (CTRL2_G) to  0001
  bit_mask = 0xF << 4;
  value = 1 << 4;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x11, bit_mask, value);

  // allowing the interruption Accelerometer Data Ready on INT1
  // set bit 0 of register 0D (INT1_CTRL) to 1
  bit_mask = 0x3;
  value = 0x3;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x0D, bit_mask, value);
}

static int setup_gpio_irq_and_workqueue() {
  /*
   * Checking that the GPIO is ready
   * configuring it as an input with an interrupt.
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

  // Adding a callback to select the interrupt handler.
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
  k_work_init(&handle_new_data_job, handle_new_data);

  k_work_queue_init(&my_work_q);

  k_work_queue_start(&my_work_q, my_stack_area,
                     K_THREAD_STACK_SIZEOF(my_stack_area), MY_PRIORITY, NULL);

  LOG_INF("Irq setup was successful\n");
  return 1;
}
