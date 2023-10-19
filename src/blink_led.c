#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#ifndef BLINK_LED_H
#define BLINK_LED_H
#include "blink_led.h"
#endif

#define MSGQ_CHECK_PERIOD K_MSEC(MSGQ_CHECK_PERIOD_MS)

extern struct k_msgq blink_period_msgq;

static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
                                                     {0});

static void check_msgq_during_half_period();
static int get_message(blink_half_period_ms_t *blink_period_ptr);
static int setup_led_device();

void led_main()
{
  if (!setup_led_device())
    return;

  // Blinking the led and checking the message queue
  while (1)
  {
    gpio_pin_set_dt(&led, 1);
    check_msgq_during_half_period();
    gpio_pin_set_dt(&led, 0);
    check_msgq_during_half_period();
  }
}

/*
 * Check the message queue every MSGQ_CHECK_PERIOD
 * for a total duration of a half period.
 * Update curr_blink_period and break if necessary.
 */
static void check_msgq_during_half_period()
{
  static blink_half_period_ms_t curr_blink_period = SLOW_BLINK;
  static blink_half_period_ms_t new_blink_period;
  
  new_blink_period = curr_blink_period;

  for (int i = 0; i < curr_blink_period / MSGQ_CHECK_PERIOD_MS; i++)
  {
    // Check the message queue every MSGQ_CHECK_PERIOD
    k_sleep(MSGQ_CHECK_PERIOD);
    int new_msg = get_message(&new_blink_period);
    if (new_msg && curr_blink_period != new_blink_period)
    {
      curr_blink_period = new_blink_period;
      break;
    }
  }
}

/*
 * Put the content of a message in blink_period_ptr.
 * Return 1 if successful.
 */
static int get_message(blink_half_period_ms_t *blink_period_ptr)
{
  return !k_msgq_get(&blink_period_msgq, blink_period_ptr, K_NO_WAIT);
}

/*
 * Setup the led device.
 * Return 1 if successful
 */
static int setup_led_device()
{
  if (!gpio_is_ready_dt(&led))
  {
    printk("Error : LED device %s is not ready; ignoring it\n",
           led.port->name);
    return 0;
  }
  else if (!led.port)
  {
    printk("Error : LED port not found\n");
    return 0;
  }
  else
  {
    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
    if (ret != 0)
    {
      printk("Error %d: failed to configure LED device %s pin %d\n",
             ret, led.port->name, led.pin);
      return 0;
    }
    else
    {
      printk("Set up LED at %s pin %d\n", led.port->name, led.pin);
      return 1;
    }
  }
}