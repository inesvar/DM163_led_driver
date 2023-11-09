#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DM163_NODE DT_NODELABEL(dm163)
static const struct device *dm163_dev = DEVICE_DT_GET(DM163_NODE);

int main() {
  if (!device_is_ready(dm163_dev)) {
    return -ENODEV;
  }
  // Set brightness to 5% for all leds so that we don't become blind
  for (int i = 0; i < 8; i++) led_set_brightness(dm163_dev, i, 5);
  // Animate the leds
  for (;;) {
    for (int col = 0; col < 8; col++) {
      led_on(dm163_dev, col);
      k_sleep(K_MSEC(100));
      led_off(dm163_dev, col);
    }
  }
}
