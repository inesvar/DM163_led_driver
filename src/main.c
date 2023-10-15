/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#define SLEEP_TIME_MS 1
#define TURN_OFF_LED_TASK_DELAY_MS K_MSEC(1000)

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
                                                              {0});
static struct gpio_callback button_cb_data;

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
                                                     {0});

/*
 * This task turns off the led one second after the button press.
 */
static struct k_work_delayable turn_off_led_task;

void turn_off_led(struct k_work *task)
{
  gpio_pin_set_dt(&led, 0);
  printk("Led turned off at %" PRIu32 "\n", k_cycle_get_32());
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
  gpio_pin_set_dt(&led, 1);
  k_work_reschedule(&turn_off_led_task, TURN_OFF_LED_TASK_DELAY_MS);
  printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
}

int main(void)
{
  int ret;

  if (!gpio_is_ready_dt(&button))
  {
    printk("Error: button device %s is not ready\n",
           button.port->name);
    return 0;
  }

  ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
  if (ret != 0)
  {
    printk("Error %d: failed to configure %s pin %d\n",
           ret, button.port->name, button.pin);
    return 0;
  }

  ret = gpio_pin_interrupt_configure_dt(&button,
                                        GPIO_INT_EDGE_TO_ACTIVE);
  if (ret != 0)
  {
    printk("Error %d: failed to configure interrupt on %s pin %d\n",
           ret, button.port->name, button.pin);
    return 0;
  }

  gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
  gpio_add_callback(button.port, &button_cb_data);
  printk("Set up button at %s pin %d\n", button.port->name, button.pin);

  if (led.port && !gpio_is_ready_dt(&led))
  {
    printk("Error %d: LED device %s is not ready; ignoring it\n",
           ret, led.port->name);
    led.port = NULL;
  }
  if (led.port)
  {
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
    if (ret != 0)
    {
      printk("Error %d: failed to configure LED device %s pin %d\n",
             ret, led.port->name, led.pin);
      led.port = NULL;
    }
    else
    {
      printk("Set up LED at %s pin %d\n", led.port->name, led.pin);
    }
  }

  if (!led.port)
  {
    return 0;
  }

  k_work_init_delayable(&turn_off_led_task, turn_off_led);
  printk("Initialized the delayable work item \"turn_off_led_task\"\n");

  printk("Press the button\n");
  return 0;
}
