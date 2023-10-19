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

static int setup_tof_sensor();

int main(void)
{
  if (!setup_tof_sensor())
    return 1;

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

static int setup_tof_sensor() {
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
    return 1;
  }
}
