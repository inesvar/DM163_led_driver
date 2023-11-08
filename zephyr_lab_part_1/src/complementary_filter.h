#include <zephyr/kernel.h>

typedef struct {
  struct k_mutex mutex;
  volatile double value;
} protected_variable;

extern protected_variable tilt_from_acceleration;
extern protected_variable tilt_change_from_gyroscope;

void compute_board_attitude_with_filter();
void init_complementary_filter();