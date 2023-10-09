#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

// Designate the `led0` alias from the device tree
#define LED_NODE DT_ALIAS(led0)

// Get the GPIO (port, pin and flags) corresponding to the
// `led0` node alias at compile time.
static const struct gpio_dt_spec led_gpio = GPIO_DT_SPEC_GET(LED_NODE, gpios);

int main() {
  if (!device_is_ready(led_gpio.port)) {
    return -ENODEV;
  }
  // Note that we use `INACTIVE`, not `LOW`. On some boards,
  // the behaviour of the output may be reversed, but this
  // is not our concern. This info belongs to the device tree.
  gpio_pin_configure_dt(&led_gpio, GPIO_OUTPUT_INACTIVE);
  for (;;) {
    gpio_pin_toggle_dt(&led_gpio);
    k_sleep(K_SECONDS(1));
  }
  return 0;
}

