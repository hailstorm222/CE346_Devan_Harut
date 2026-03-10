#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_gpio.h"

typedef struct {
    uint32_t pin;
    nrf_gpio_pin_pull_t pull;
    bool active_high;
} touch_sensor_t;

void touch_sensor_init(touch_sensor_t *sensor, uint32_t pin);
void touch_sensor_init_with_config(touch_sensor_t *sensor,
                                   uint32_t pin,
                                   nrf_gpio_pin_pull_t pull,
                                   bool active_high);

bool touch_sensor_is_touched(const touch_sensor_t *sensor);

void touch_sensor_init_array(touch_sensor_t *sensors,
                             const uint32_t *pins,
                             size_t count);

size_t touch_sensor_read_array(const touch_sensor_t *sensors,
                               bool *states,
                               size_t count);
