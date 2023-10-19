#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#ifndef BLINK_LED_H
#define BLINK_LED_H
#include "blink_led.h"
#endif

#define MSGQ_BUFFER_SIZE 1
#define MSGQ_UPDATE_PERIOD K_MSEC(MSGQ_CHECK_PERIOD_MS)

#define LED_THREAD_STACK_SIZE 512
#define LED_THREAD_PRIORITY 15

/*
 * Define the message queue and its buffer.
 */
struct k_msgq blink_period_msgq;
static uint8_t blink_period_msgq_buffer[sizeof(blink_half_period_ms_t) * MSGQ_BUFFER_SIZE];

/*
 * Define a secondary thread.
 */
static struct k_thread led_thread;

/*
 * Get the Time-of-Flight sensor.
 */
static const struct device *const tof_sensor = DEVICE_DT_GET_ANY(st_vl53l0x);
K_THREAD_STACK_DEFINE(led_thread_stack, LED_THREAD_STACK_SIZE);

static blink_half_period_ms_t get_blink_period_from_measure();
static void update_msgq_if_necessary(blink_half_period_ms_t new_blink_period);
static void send_message();
static int setup_tof_sensor();

int main(void)
{
  if (!setup_tof_sensor())
    return 1;

  // Initializing the message queue
  k_msgq_init(&blink_period_msgq, blink_period_msgq_buffer, 
                          sizeof(blink_half_period_ms_t), MSGQ_BUFFER_SIZE);

  // Spawning a thread for the led task
  k_thread_create(&led_thread, led_thread_stack, LED_THREAD_STACK_SIZE, 
                          led_main, NULL, NULL, NULL, 
                          LED_THREAD_PRIORITY, 0, K_NO_WAIT);

  printk("Move your hand above the sensor\n");
  while (1)
  {
    blink_half_period_ms_t new_blink_period = get_blink_period_from_measure();
    update_msgq_if_necessary(new_blink_period);
    k_sleep(MSGQ_UPDATE_PERIOD);
  }

  return 0;
}

static blink_half_period_ms_t get_blink_period_from_measure() {
    struct sensor_value distance;
    sensor_sample_fetch(tof_sensor);
    sensor_channel_get(tof_sensor, SENSOR_CHAN_DISTANCE, &distance);

    if (!distance.val1 && distance.val2 < 200000)
    {
      return FAST_BLINK;
    }
    else
    {
      return SLOW_BLINK;
    } 
}

static void update_msgq_if_necessary(blink_half_period_ms_t new_blink_period)
{
  static blink_half_period_ms_t prev_blink_period = SLOW_BLINK;
  static blink_half_period_ms_t curr_blink_period;

  curr_blink_period = new_blink_period;
  if (prev_blink_period != curr_blink_period)
  {
    send_message(&curr_blink_period);
  }
  prev_blink_period = curr_blink_period;
}

static void send_message(blink_half_period_ms_t *blink_period_ptr)
{
  // if the message queue is full
  if (k_msgq_num_free_get(&blink_period_msgq) == 0)
  {
    k_msgq_purge(&blink_period_msgq);
  }
  k_msgq_put(&blink_period_msgq, blink_period_ptr, K_NO_WAIT);
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
