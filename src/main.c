#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#define ACCELEROMETER_NODE DT_ALIAS(accel0)
static struct i2c_dt_spec accelerometer_i2c = I2C_DT_SPEC_GET(ACCELEROMETER_NODE);

int main(void)
{
  if (!i2c_is_ready_dt(&accelerometer_i2c))
  {
    printk("Accelerometer isn't ready\n");
    return 1;
  }

  uint8_t accelerometer_read_address = (accelerometer_i2c.addr << 1) | 1;
  printk("Accelerometer is ready, read address on I2C bus is 0x%x\n", accelerometer_read_address);
  uint8_t who_am_I = 0;
  i2c_reg_read_byte_dt(&accelerometer_i2c, 0x0F, &who_am_I);

  printk("Content of who_am_I register : 0x%x\n", who_am_I);
  return 0;
}