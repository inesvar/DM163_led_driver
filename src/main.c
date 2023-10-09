#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

// Designate the `leds` node from the device tree
#define LED_NODE DT_PATH(leds)

// Get the struct device pointer corresponding to the
// `leds` node at compile time.
static const struct device * const led_device = DEVICE_DT_GET(LED_NODE);

int main() {
  if (!device_is_ready(led_device)) {
    return -ENODEV;
  }
  for (;;) {
    // switch on LD1, switch off LD2
    led_on(led_device, 0);
    led_off(led_device, 1);
    k_sleep(K_SECONDS(1)); 

    // switch off LD1, switch on LD2
    led_off(led_device, 0);
    led_on(led_device, 1);
    k_sleep(K_SECONDS(1));
  }
  return 0;
}



