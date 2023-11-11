#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

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

int main() {
  if (!device_is_ready(dm163_dev)) {
    return -ENODEV;
  }
  for (int row = 0; row < 8; row++)
    gpio_pin_configure_dt(&rows[row], GPIO_OUTPUT_INACTIVE);
  // Set brightness to 5% for all leds so that we don't become blind
  for (int i = 0; i < 8; i++) led_set_brightness(dm163_dev, i, 5);
  // Animate the leds on every row and every column
  for (int row = 0; row < 8; row++) {
    gpio_pin_set_dt(&rows[row], 1);
  }
}
