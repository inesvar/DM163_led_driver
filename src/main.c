#include <inttypes.h>
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
 * Accelerometer interrupt callback data
 */
static struct gpio_callback acceleration_isr_data;

/*
 * Workqueue job to print the acceleration, submitted from interrupt
 */
static struct k_work print_acceleration_job;

/*
 * Linear acceleration register address, values and axis labels
 */
static uint8_t acceleration_register_address = 0x28;
static uint8_t acceleration_register[6];
static char acceleration_axis_label[3] = {'X', 'Y', 'Z'};

static void acceleration_isr(const struct device *dev, struct gpio_callback *cb,
                             uint32_t pins);
static void configure_accelerometer();
static int setup_gpio_irq_and_workqueue();
static void print_acceleration();

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
 * Submit a print acceleration job to the workqueue
 */
static void acceleration_isr(const struct device *dev, struct gpio_callback *cb,
                             uint32_t pins) {
  k_work_submit(&print_acceleration_job);
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

  // set output data rate of accelerometer at 1.6Hz
  // set bits [7:4] of register 10 (CTRL1_XL) to 1011
  bit_mask = 0xF << 4;
  value = 0xB << 4;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x10, bit_mask, value);

  // allowing the interruption Accelerometer Data Ready on INT1
  // set bit 0 of register 0D (INT1_CTRL) to 1
  bit_mask = 1;
  value = 1;
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
  gpio_init_callback(&acceleration_isr_data, acceleration_isr,
                     BIT(accelerometer_irq_gpio.pin));

  if (gpio_add_callback(accelerometer_irq_gpio.port, &acceleration_isr_data)) {
    LOG_ERR("Callback couldn't be added\n");
    return 0;
  }

  /*
   * Initializing a workqueue job to execute the blocking I2C
   * operation to read the acceleration.
   */
  k_work_init(&print_acceleration_job, print_acceleration);

  LOG_INF("Irq setup was successful\n");
  return 1;
}

static void print_acceleration() {
  static int measure_count = 0;

  while (gpio_pin_get_dt(&accelerometer_irq_gpio)) {
    LOG_DBG("measure number %d\n", measure_count);
    // read the contents of all 6 acceleration registers
    // put their contents in a buffer
    i2c_write_read_dt(&accelerometer_i2c, &acceleration_register_address, 1,
                      acceleration_register, 6);

    for (int i = 0; i < 3; i++) {
      uint16_t acceleration = acceleration_register[i * 2] |
                              (acceleration_register[2 * i + 1] << 8);
      int16_t signed_acceleration = (int16_t)acceleration;
      LOG_PRINTK("%c axis acceleration %hd (1g = 16384)\n",
                 acceleration_axis_label[i], signed_acceleration);
    }
    LOG_PRINTK("\n");
    measure_count++;
  }
}
