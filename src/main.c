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
#define TURN_OFF_LED_TASK_DELAY_MS K_MSEC(3000)

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
 * The context of the task that will turn off the led one second
 * after the button press.
 */

struct work_context
{
  struct k_work_delayable task;
  volatile int ignore_next_task;
};

static struct work_context led_task_ctx;

void turn_off_led(struct k_work *work)
{
  printk("\nStarting task...\n");
  k_sleep(K_MSEC(1000));
  struct k_work_delayable *dwork = k_work_delayable_from_work(work);
  struct work_context *ctx = CONTAINER_OF(dwork, struct work_context,
                                          task);
  // disabling the button interrupt
  // interrupt 40 : EXTI lines 10 to 15
  irq_disable(40);
  printk("Entering critical section\n");
  k_sleep(K_MSEC(1000));
  if (ctx->ignore_next_task == 0)
  {
    gpio_pin_set_dt(&led, 0);
    printk("Led turned off at %" PRIu32 "\n", k_cycle_get_32());
  }
  else
  {
    ctx->ignore_next_task = 0;
  }
  printk("Exiting critical section\n");
  irq_enable(40);
  k_sleep(K_MSEC(1000));
  printk("Exiting task...\n");
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
  printk("\nButton pressed at %" PRIu32 "\n", k_cycle_get_32());
  // Ensuring the task will not switch off the led right after this interrupt
  // in other words : if the task is running and the led is on,
  // set a flag preventing normal execution
  int result = k_work_delayable_busy_get(&led_task_ctx.task);
  if ((result & K_WORK_RUNNING) && gpio_pin_get_dt(&led))
  {
    printk("turn_off_led_task is running and would have turned off the led prematurely.\n");
    led_task_ctx.ignore_next_task = 1;
  }
  // Turn on the light and reschedule the turn off led task
  gpio_pin_set_dt(&led, 1);
  k_work_reschedule(&led_task_ctx.task, TURN_OFF_LED_TASK_DELAY_MS);
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

  k_work_init_delayable(&led_task_ctx.task, turn_off_led);
  printk("Initialized the work context \"led_task_ctx\"\n");

  printk("Press the button\n");
  return 0;
}
