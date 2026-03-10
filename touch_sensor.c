#include "touch_sensor.h"

void touch_sensor_init(touch_sensor_t *sensor, uint32_t pin) {
    touch_sensor_init_with_config(sensor, pin, NRF_GPIO_PIN_PULLDOWN, true);
}

void touch_sensor_init_with_config(touch_sensor_t *sensor,
                                   uint32_t pin,
                                   nrf_gpio_pin_pull_t pull,
                                   bool active_high) {
    if (sensor == NULL) {
        return;
    }

    sensor->pin = pin;
    sensor->pull = pull;
    sensor->active_high = active_high;

    nrf_gpio_cfg_input(sensor->pin, sensor->pull);
}

bool touch_sensor_is_touched(const touch_sensor_t *sensor) {
    if (sensor == NULL) {
        return false;
    }

    bool level_high = nrf_gpio_pin_read(sensor->pin) != 0;
    return sensor->active_high ? level_high : !level_high;
}

void touch_sensor_init_array(touch_sensor_t *sensors,
                             const uint32_t *pins,
                             size_t count) {
    if ((sensors == NULL) || (pins == NULL)) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        touch_sensor_init(&sensors[i], pins[i]);
    }
}

size_t touch_sensor_read_array(const touch_sensor_t *sensors,
                               bool *states,
                               size_t count) {
    size_t touched_count = 0;

    if ((sensors == NULL) || (states == NULL)) {
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        states[i] = touch_sensor_is_touched(&sensors[i]);
        if (states[i]) {
            ++touched_count;
        }
    }

    return touched_count;
}
