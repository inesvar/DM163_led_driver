#include "spirit_level.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/*
 * Defining the accelerometer
 */
#define ACCELEROMETER_NODE DT_ALIAS(accel0)

static struct i2c_dt_spec accelerometer_i2c =
    I2C_DT_SPEC_GET(ACCELEROMETER_NODE);

static const struct gpio_dt_spec accelerometer_irq_gpio =
    GPIO_DT_SPEC_GET(ACCELEROMETER_NODE, irq_gpios);

#define ACCELEROMETER_ODR 52

/*
 * Linear acceleration register address and values
 */
static uint8_t acceleration_register_address = 0x28;
static int16_t acceleration_measure[2];
static uint8_t acceleration_register[4];

/*
 * Accelerometer new data ready
 */

// interrupt data
static struct gpio_callback acceleration_isr_data;
// update velocity job
static struct k_work update_velocity_job;

/*
 * Creating a PrecisePosition struct
 */
struct PrecisePosition {
  double x;
  double y;
  double v_x;
  double v_y;
};

// 2.999...9 is rounded to 2 and 4.000..1 is rounded to 4
// so 3.5 is the mean of the values rounded to 3
static struct PrecisePosition precise_position = {3.5, 3.5, 0, 0};

/*
 * The precise position is approximated on the led matrix by
 * these two arrays
 */
uint8_t actual_position[2];
uint8_t previous_position[2];

uint8_t approximate_on_led_matrix(double value);

LOG_MODULE_REGISTER(spirit_level, LOG_LEVEL_INF);

// update the velocity of the spirit from the acceleration
void update_velocity() {
  // read the accelerometer registers and update the velocity
  while (gpio_pin_get_dt(&accelerometer_irq_gpio)) {
    // read the contents of the 4 needed acceleration registers
    // put their contents in a buffer
    i2c_write_read_dt(&accelerometer_i2c, &acceleration_register_address, 1,
                      acceleration_register, 4);

    // get the acceleration measures from register contents
    for (int i = 0; i < 2; i++) {
      uint16_t acceleration = acceleration_register[i * 2] |
                              (acceleration_register[i * 2 + 1] << 8);
      acceleration_measure[i] = (int16_t)acceleration;
    }

    // update velocity using acceleration
    // acc
    precise_position.v_x -= (double)(acceleration_measure[1]) /
                            ACCELEROMETER_ODR / ACCELERATION_DIVIDER;
    precise_position.v_y -= (double)(acceleration_measure[0]) /
                            ACCELEROMETER_ODR / ACCELERATION_DIVIDER;

    LOG_DBG("accel x %hd ; accel y %hd\n", acceleration_measure[0],
            acceleration_measure[1]);
    LOG_DBG("from accel : v_x %g ; v_y %g\n\n", precise_position.v_x,
            precise_position.v_y);
  }
}

/*
 * Takes a value in [0, 8] and outputs an uint8_t between 0 and 7 included
 */
uint8_t approximate_on_led_matrix(double value) {
  return ((uint8_t)value == 8) ? 7 : (uint8_t)value;
}

// update the position of the spirit from the velocity
uint8_t update_position_get_spirit_row() {
  previous_position[0] = approximate_on_led_matrix(precise_position.x);
  previous_position[1] = approximate_on_led_matrix(precise_position.y);

  precise_position.x += precise_position.v_x / FPS;
  precise_position.y += precise_position.v_y / FPS;

  /*
   * In case the spirit level reached the side of the board,
   * the position and velocity needs to be adapted
   */
  if (precise_position.x > 8) {
    precise_position.v_x -= (precise_position.x - 8) * FPS;
    precise_position.x = 8;
  } else if (precise_position.x < 0) {
    precise_position.v_x -= precise_position.x * FPS;
    precise_position.x = 0;
  }

  if (precise_position.y > 8) {
    precise_position.v_y -= (precise_position.y - 8) * FPS;
    precise_position.y = 8;
  } else if (precise_position.y < 0) {
    precise_position.v_y -= precise_position.y * FPS;
    precise_position.y = 0;
  }

  actual_position[0] = approximate_on_led_matrix(precise_position.x);
  actual_position[1] = approximate_on_led_matrix(precise_position.y);

  LOG_DBG("x %g ; y %g\n\n", precise_position.x, precise_position.y);
  LOG_DBG("v_x %g ; v_y %g\n\n", precise_position.v_x, precise_position.v_y);

  return actual_position[1];
}

void update_channels(uint8_t channels[24]) {
  channels[3 * previous_position[0]] = 0;
  channels[3 * previous_position[0] + 1] = 0;
  channels[3 * previous_position[0] + 2] = 0;

  channels[3 * actual_position[0]] = 255;
  channels[3 * actual_position[0] + 1] = 255;
  channels[3 * actual_position[0] + 2] = 255;
}

/*
 * Accelerometer Interrupt Handler
 */
static void acceleration_isr(const struct device *dev, struct gpio_callback *cb,
                             uint32_t pins) {
  k_work_submit(&update_velocity_job);
}

void configure_accelerometer() {
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

  // allowing the interruption Accelerometer Data Ready on INT1
  // set bit 0 of register 0D (INT1_CTRL) to 1
  bit_mask = 1;
  value = 1;
  i2c_reg_update_byte_dt(&accelerometer_i2c, 0x0D, bit_mask, value);
}

int setup_accelerometer_irq() {
  if (!i2c_is_ready_dt(&accelerometer_i2c)) {
    LOG_ERR("Accelerometer i2c isn't ready\n");
    return 0;
  }
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
   * operation to read the acceleration and update the velocity
   */
  k_work_init(&update_velocity_job, update_velocity);

  LOG_INF("Irq setup was successful\n");
  return 1;
}