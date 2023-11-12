#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DM163_NODE DT_NODELABEL(dm163)
static const struct device *dm163_dev = DEVICE_DT_GET(DM163_NODE);

#define RGB_MATRIX_NODE DT_NODELABEL(rgb_matrix)

BUILD_ASSERT(DT_PROP_LEN(RGB_MATRIX_NODE, rows_gpios) == 8);

static const struct gpio_dt_spec rows[] = {
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 0),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 1),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 2),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 3),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 4),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 5),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 6),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 7),
};
/*
 * Defining a semaphore to hold the display thread for a time
 * before displaying the next line
 */
struct k_sem display_next_line;

K_SEM_DEFINE(display_next_line, 0, 1);

/*
 * Defining a timer allowing the display of the next line 80 * 6 per second
 * by signaling the semaphore periodically
 */
struct k_timer next_line_timer;

static void allow_display_next_line(struct k_timer *timer_id) {
  k_sem_give(&display_next_line);
}
K_TIMER_DEFINE(next_line_timer, allow_display_next_line, NULL);

k_timeout_t display_next_line_period = K_USEC(1000000 / 8 / 60);

/*
 * Creating an Image struct to hold an entire 8x8 image
 */
struct Image {
  uint8_t channels[8][8][3];
};

/*
 * Useful objects to create moving car images
 */
#define MOVING_CAR_NB_IMAGES 5
static struct Image moving_car[MOVING_CAR_NB_IMAGES];

static uint8_t BLUE_SKY[3] = {19, 134, 196};
static uint8_t GREY_TIRES[3] = {24, 24, 24};
static uint8_t RED_CAR[3] = {255, 0, 0};
static uint8_t WHITE_STRIPE[3] = {255, 255, 140};

static void color_led(int image_idx, int row, int col, uint8_t color[3]);
static void draw_moving_car_images();
static void display_moving_image(struct Image *moving_image, int nb_images);

int main() {
  if (!device_is_ready(dm163_dev)) {
    return -ENODEV;
  }
  for (int row = 0; row < 8; row++)
    gpio_pin_configure_dt(&rows[row], GPIO_OUTPUT_INACTIVE);
  // Set brightness to 5% for all leds so that we don't become blind
  for (int i = 0; i < 8; i++) led_set_brightness(dm163_dev, i, 5);
  // Draw images
  draw_moving_car_images();
  // Setup the timer to allow the display of a new line periodically
  k_timer_start(&next_line_timer, K_NO_WAIT, display_next_line_period);
  // Launch the displaying task
  display_moving_image((struct Image *)moving_car, MOVING_CAR_NB_IMAGES);
}

static void display_moving_image(struct Image *moving_image, int nb_images) {
  int nb_frames_per_image = 20;
  while (1) {
    for (int idx = 0; idx < nb_frames_per_image * nb_images; idx++) {
      for (int row = 0; row < 8; row++) {
        if (k_sem_take(&display_next_line, K_FOREVER) == 0) {
          gpio_pin_set_dt(&rows[(row + 7) % 8], 0);
          led_write_channels(
              dm163_dev, 0, 24,
              (uint8_t *)moving_image[idx / nb_frames_per_image].channels[row]);
          gpio_pin_set_dt(&rows[row], 1);
        }
      }
    }
  }
}

static void draw_moving_car_images() {
  for (int idx = 0; idx < MOVING_CAR_NB_IMAGES; idx++) {
    for (int row = 0; row < 4; row++) {
      for (int col = 0; col < 8; col++) {
        color_led(idx, row, col, BLUE_SKY);
      }
    }

    for (int col = 3; col < 6; col++) {
      color_led(idx, 2, col, RED_CAR);
    }

    for (int col = 2; col < 7; col++) {
      color_led(idx, 3, col, RED_CAR);
    }

    for (int col = 1; col < 7; col++) {
      color_led(idx, 4, col, RED_CAR);
    }

    color_led(idx, 5, 2, GREY_TIRES);
    color_led(idx, 5, 5, GREY_TIRES);

    if (idx > 0 && idx < MOVING_CAR_NB_IMAGES - 1) {
      color_led(idx, 6, 2 * idx - 1, WHITE_STRIPE);
      color_led(idx, 6, 2 * idx, WHITE_STRIPE);
    }
  }

  color_led(0, 6, 0, WHITE_STRIPE);
  color_led(MOVING_CAR_NB_IMAGES - 1, 6, 7, WHITE_STRIPE);
}

static void color_led(int image_idx, int row, int col, uint8_t color[3]) {
  moving_car[image_idx].channels[row][col][0] = color[0];
  moving_car[image_idx].channels[row][col][1] = color[1];
  moving_car[image_idx].channels[row][col][2] = color[2];
}
