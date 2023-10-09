#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

// Define `GPIOA_NODE` as the node whose label is `gpioa` in the device tree
#define GPIOA_NODE DT_NODELABEL(gpioa)

// Get the struct device pointer to `gpioa` at compile time
static const struct device * const gpioa_dev = DEVICE_DT_GET(GPIOA_NODE);

int main() {
  if (!device_is_ready(gpioa_dev)) {
    return -ENODEV;  // Device is not ready
  }
  // Configure the led as an output, initially inactive
  gpio_pin_configure(gpioa_dev, 5, GPIO_OUTPUT_INACTIVE);
  // Toggle the led every second
  for (;;) {
    gpio_pin_toggle(gpioa_dev, 5);
    k_sleep(K_SECONDS(1));
  }
  return 0;
}
