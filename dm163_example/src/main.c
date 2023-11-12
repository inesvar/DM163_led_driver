#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#include "../dm163_module/zephyr/dm163.h"
#include "spirit_level.h"

/*
 * Defining the led matrix device and rows
 */

#define DM163_NODE DT_NODELABEL(dm163)
static const struct device *dm163_dev = DEVICE_DT_GET(DM163_NODE);

#define RGB_MATRIX_NODE DT_NODELABEL(rgb_matrix)

BUILD_ASSERT(DT_PROP_LEN(RGB_MATRIX_NODE, rows_gpios) == 8);

static const struct gpio_dt_spec rows[] = {
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 0),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 1),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 2),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 3),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 4),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 5),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 6),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 7),
};

/*
 * Defining a semaphore to hold the display thread for a time
 * before displaying the next position
 */
struct k_sem display_next_position_sem;

K_SEM_DEFINE(display_next_position_sem, 0, 1);

/*
 * Defining a timer allowing the display of the next position FPS times per
 * second by signaling the semaphore periodically
 */
struct k_timer next_position_timer;

static void allow_display_next_position(struct k_timer *timer_id) {
  k_sem_give(&display_next_position_sem);
}
K_TIMER_DEFINE(next_position_timer, allow_display_next_position, NULL);

k_timeout_t display_next_position_period = K_USEC(1000000 / FPS);

// contains the channels values for the next display
static uint8_t channels[24];
// displays a white led on the position of the spirit level
static void display_position();

int main() {
  if (!device_is_ready(dm163_dev)) {
    return -ENODEV;
  }

  if (!setup_accelerometer_irq()) {
    return 1;
  }

  configure_accelerometer();

  for (int row = 0; row < 8; row++)
    gpio_pin_configure_dt(&rows[row], GPIO_OUTPUT_INACTIVE);
  // Set brightness to 5% for all leds so that we don't become blind
  for (int i = 0; i < 8; i++) led_set_brightness(dm163_dev, i, 5);

  // Setup the timer to allow the display of a new position periodically
  k_timer_start(&next_position_timer, K_NO_WAIT, display_next_position_period);

  display_position();
}

static void display_position() {
  while (1) {
    if (k_sem_take(&display_next_position_sem, K_FOREVER) == 0) {
      uint8_t actual_row = update_position_get_spirit_row();
      update_channels(channels);

      led_write_channels(dm163_dev, 0, 24, (uint8_t *)&channels);
      gpio_pin_set_dt(&rows[actual_row], 1);
      dm163_turn_off_row(dm163_dev, &rows[actual_row]);
    }
  }
}