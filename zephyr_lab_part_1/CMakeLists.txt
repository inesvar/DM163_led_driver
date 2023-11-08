cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(my_zephyr_app)

target_sources(app PRIVATE src/main.c PRIVATE src/handle_data.c PRIVATE src/complementary_filter.c PRIVATE src/blink_leds.c)
