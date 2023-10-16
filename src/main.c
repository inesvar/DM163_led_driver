/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#define SLEEP_TIME_MS 1
#define SLEEP_TIME_BETWEEN_MEASURES_MS K_MSEC(1000)

/*
 * Get the Time-of-Flight sensor.
 */
static const struct device *const tof_sensor = DEVICE_DT_GET_ANY(st_vl53l0x);

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
                                                     {0});

int main(void)
{
  // Checking for led errors
  if (led.port && !gpio_is_ready_dt(&led))
  {
    printk("Error : LED device %s is not ready; ignoring it\n",
           led.port->name);
    return 0;
  }
  else if (led.port)
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
    }
  }

  // Checking for sensor erros
  if (tof_sensor == NULL)
  {
    printk("TOF sensor wasn't found.\n");
    return 0;
  }
  else if (!device_is_ready(tof_sensor))
  {
    printk("TOF sensor isn't ready.\n");
    return 0;
  }
  else
  {
    printk("TOF sensor found and ready.\n");
  }

  // Printing the distance measures from the tof sensor
  while (1)
  {
    struct sensor_value distance;
    sensor_sample_fetch(tof_sensor);
    sensor_channel_get(tof_sensor, SENSOR_CHAN_DISTANCE, &distance);
    printk("distance: %d.%06d\n", distance.val1, distance.val2);
    k_sleep(SLEEP_TIME_BETWEEN_MEASURES_MS);
  }

  return 0;
}
