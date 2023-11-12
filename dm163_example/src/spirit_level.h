#ifndef SPIRIT_LEVEL_H
#define SPIRIT_LEVEL_H

#include <inttypes.h>

#define FPS 60
// the acceleration is divided by this coefficient so it's not too high
#define ACCELERATION_DIVIDER 50

uint8_t update_position_get_spirit_row();
void update_channels(uint8_t channels[24]);
void update_velocity();

void configure_accelerometer();
int setup_accelerometer_irq();

#endif