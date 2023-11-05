#ifndef HANDLE_DATA_H
#define HANDLE_DATA_H

#define ACCELEROMETER_ODR 52
#define GYROSCOPE_ODR 52

extern struct k_work handle_new_data_job;

extern void handle_new_data();

#endif