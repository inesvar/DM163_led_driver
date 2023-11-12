// Enter the driver name so that when we initialize the (maybe numerous)
// DM163 peripherals we can designated them by index.
#define DT_DRV_COMPAT siti_dm163

#include "dm163.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dm163, LOG_LEVEL_DBG);

struct dm163_config {
  const struct gpio_dt_spec en;
  const struct gpio_dt_spec gck;
  const struct gpio_dt_spec lat;
  const struct gpio_dt_spec rst;
  const struct gpio_dt_spec selbk;
  const struct gpio_dt_spec sin;
};

#define NUM_LEDS 8
#define NUM_CHANNELS (NUM_LEDS * 3)

struct dm163_data {
  uint8_t brightness[NUM_CHANNELS];
  uint8_t channels[NUM_CHANNELS];
  struct k_mutex flush_mutex;
};

static const struct gpio_dt_spec *row_to_turn_off;

static int dm163_set_color(const struct device *dev, uint32_t led,
                           uint8_t num_colors, const uint8_t *color);
static int dm163_write_channels(const struct device *dev,
                                uint32_t start_channel, uint32_t num_channels,
                                const uint8_t *buf);
static int dm163_set_brightness(const struct device *dev, uint32_t led,
                                uint8_t value);
static int dm163_on(const struct device *dev, uint32_t led);
static int dm163_off(const struct device *dev, uint32_t led);
static void flush_brightness(const struct device *dev);
static void flush_channels(const struct device *dev);

#define CONFIGURE_PIN(dt, flags)                           \
  do {                                                     \
    if (!device_is_ready((dt)->port)) {                    \
      LOG_ERR("device %s is not ready", (dt)->port->name); \
      return -ENODEV;                                      \
    }                                                      \
    gpio_pin_configure_dt(dt, flags);                      \
  } while (0)

static int dm163_init(const struct device *dev) {
  const struct dm163_config *config = dev->config;
  struct dm163_data *data = dev->data;
  k_mutex_init(&data->flush_mutex);

  LOG_DBG("starting initialization of device %s", dev->name);

  // Disable DM163 outputs while configuring if this pin
  // is connected.
  if (config->en.port) {
    CONFIGURE_PIN(&config->en, GPIO_OUTPUT_INACTIVE);
  }
  // Configure all pins. Make reset active so that the DM163
  // initiates a reset. We want the clock (gck) and latch (lat)
  // to be inactive at start. selbk will select bank 1 by default.
  CONFIGURE_PIN(&config->rst, GPIO_OUTPUT_ACTIVE);
  CONFIGURE_PIN(&config->gck, GPIO_OUTPUT_INACTIVE);
  CONFIGURE_PIN(&config->lat, GPIO_OUTPUT_INACTIVE);
  CONFIGURE_PIN(&config->selbk, GPIO_OUTPUT_ACTIVE);
  CONFIGURE_PIN(&config->sin, GPIO_OUTPUT);
  k_usleep(1);  // 100ns min
  // Cancel reset by making it inactive.
  gpio_pin_set_dt(&config->rst, 0);

  memset(&data->brightness, 0x3f, sizeof(data->brightness));
  memset(&data->channels, 0x00, sizeof(data->channels));
  flush_brightness(dev);
  flush_channels(dev);

  // Enable the outputs if this pin is connected.
  if (config->en.port) {
    gpio_pin_set_dt(&config->en, 1);
  }
  LOG_INF("device %s initialized", dev->name);
  return 0;
}

static const struct led_driver_api dm163_api = {
    .on = dm163_on,
    .off = dm163_off,
    .set_brightness = dm163_set_brightness,
    .set_color = dm163_set_color,
    .write_channels = dm163_write_channels,
};

// Macro to initialize the DM163 peripheral with index i
#define DM163_DEVICE(i)                                                        \
                                                                               \
  /* Build a dm163_config for DM163 peripheral with index i, named          */ \
  /* dm163_config_/i/ (for example dm163_config_0 for the first peripheral) */ \
  static const struct dm163_config dm163_config_##i = {                        \
      .en = GPIO_DT_SPEC_GET_OR(DT_DRV_INST(i), en_gpios, {0}),                \
      .gck = GPIO_DT_SPEC_GET(DT_DRV_INST(i), gck_gpios),                      \
      .lat = GPIO_DT_SPEC_GET(DT_DRV_INST(i), lat_gpios),                      \
      .rst = GPIO_DT_SPEC_GET(DT_DRV_INST(i), rst_gpios),                      \
      .selbk = GPIO_DT_SPEC_GET(DT_DRV_INST(i), selbk_gpios),                  \
      .sin = GPIO_DT_SPEC_GET(DT_DRV_INST(i), sin_gpios),                      \
  };                                                                           \
                                                                               \
  /* Build a new dm163_data_/i/ structure for dynamic data                  */ \
  static struct dm163_data dm163_data_##i = {};                                \
                                                                               \
  DEVICE_DT_INST_DEFINE(i, &dm163_init, NULL, &dm163_data_##i,                 \
                        &dm163_config_##i, POST_KERNEL,                        \
                        CONFIG_LED_INIT_PRIORITY, &dm163_api);

// Apply the DM163_DEVICE to all DM163 peripherals not marked "disabled"
// in the device tree and pass it the corresponding index.
DT_INST_FOREACH_STATUS_OKAY(DM163_DEVICE)

void dm163_turn_off_row(const struct device *dev,
                        const struct gpio_dt_spec *row) {
  row_to_turn_off = row;
}

static int dm163_set_brightness(const struct device *dev, uint32_t led,
                                uint8_t value) {
  struct dm163_data *data = dev->data;

  if (led >= NUM_LEDS) return -EINVAL;

  // converting the values from [0, 100] to [0, 63]
  value = value * 63 / 100;

  data->brightness[led * 3] = value;
  data->brightness[led * 3 + 1] = value;
  data->brightness[led * 3 + 2] = value;

  flush_brightness(dev);
  return 0;
}

static int dm163_on(const struct device *dev, uint32_t led) {
  struct dm163_data *data = dev->data;

  if (led >= NUM_LEDS) return -EINVAL;

  data->channels[led * 3] = 0xff;
  data->channels[led * 3 + 1] = 0xff;
  data->channels[led * 3 + 2] = 0xff;

  flush_channels(dev);
  return 0;
}

static int dm163_off(const struct device *dev, uint32_t led) {
  struct dm163_data *data = dev->data;

  if (led >= NUM_LEDS) return -EINVAL;

  data->channels[led * 3] = 0x00;
  data->channels[led * 3 + 1] = 0x00;
  data->channels[led * 3 + 2] = 0x00;

  flush_channels(dev);
  return 0;
}

static void pulse_data(const struct dm163_config *config, uint8_t data,
                       int bits) {
  for (int led = bits - 1; led >= 0; led--) {
    uint8_t bit = (data >> (led)) & 0x1;
    gpio_pin_set_dt(&(config->sin), bit);
    gpio_pin_set_dt(&(config->gck), 1);
    gpio_pin_set_dt(&(config->gck), 0);
  }
}

static void flush_channels(const struct device *dev) {
  const struct dm163_config *config = dev->config;
  struct dm163_data *data = dev->data;

  k_mutex_lock(&data->flush_mutex, K_FOREVER);
  for (int i = NUM_CHANNELS - 1; i >= 0; i--) {
    pulse_data(config, data->channels[i], 8);
    if (i == 2 && row_to_turn_off != NULL) {
      gpio_pin_set_dt(row_to_turn_off, 0);
      row_to_turn_off = NULL;
    }
  }
  gpio_pin_set_dt(&config->lat, 0);
  gpio_pin_set_dt(&config->lat, 1);
  k_mutex_unlock(&data->flush_mutex);
}

static void flush_brightness(const struct device *dev) {
  const struct dm163_config *config = dev->config;
  struct dm163_data *data = dev->data;

  k_mutex_lock(&data->flush_mutex, K_FOREVER);
  gpio_pin_set_dt(&config->selbk, 0);
  for (int i = NUM_CHANNELS - 1; i >= 0; i--) {
    pulse_data(config, data->brightness[i], 6);
  }
  gpio_pin_set_dt(&config->lat, 0);
  gpio_pin_set_dt(&config->lat, 1);
  gpio_pin_set_dt(&config->selbk, 1);
  k_mutex_unlock(&data->flush_mutex);
}

static int dm163_set_color(const struct device *dev, uint32_t led,
                           uint8_t num_colors, const uint8_t *color) {
  struct dm163_data *data = dev->data;

  if (led >= NUM_LEDS || num_colors > 3) return -EINVAL;

  for (uint8_t i = 0; i < 3; i++) {
    data->channels[led * 3 + i] = (i < num_colors) ? *color++ : 0;
  }

  flush_channels(dev);
  return 0;
}

static int dm163_write_channels(const struct device *dev,
                                uint32_t start_channel, uint32_t num_channels,
                                const uint8_t *buf) {
  struct dm163_data *data = dev->data;

  if (start_channel + num_channels > NUM_CHANNELS) {
    return -EINVAL;
  }

  for (uint32_t i = start_channel; i < start_channel + num_channels; i++) {
    data->channels[i] = *buf++;
  }

  flush_channels(dev);
  return 0;
}
