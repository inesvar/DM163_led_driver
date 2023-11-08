#ifndef HANDLE_DATA_H
#define HANDLE_DATA_H

#define ACCELEROMETER_ODR 52
#define GYROSCOPE_ODR 1660

extern void handle_new_data();

extern void init_filter_workq();

#endif