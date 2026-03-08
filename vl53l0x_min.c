#include "vl53l0x_min.h"

#include <stdbool.h>
#include <stdint.h>

#include "i2c_simple.h"
#include "hw_config.h"
#include "nrf_delay.h"

#define VL53L0X_REG_SYSRANGE_START      0x00
#define VL53L0X_REG_RESULT_RANGE_STATUS 0x14
#define VL53L0X_REG_RESULT_RANGE_VALUE  0x1E

bool vl53l0x_present(void) {
    return i2c_probe(VL53L0X_ADDR);
}

bool vl53l0x_read_id(uint8_t* model, uint8_t* revision) {
    if (!i2c_read_reg8(VL53L0X_ADDR, 0xC0, model)) {
        return false;
    }
    if (!i2c_read_reg8(VL53L0X_ADDR, 0xC2, revision)) {
        return false;
    }
    return true;
}

bool vl53l0x_read_distance_mm(uint16_t* distance_mm) {
    uint8_t buf[2];

    /* Start single-shot ranging */
    if (!i2c_write_reg8(VL53L0X_ADDR, VL53L0X_REG_SYSRANGE_START, 0x01)) {
        return false;
    }

    /* Wait for measurement (typically 20-30 ms) */
    nrf_delay_ms(30);

    /* Poll until done (status 0 = valid, 255 = no update means still busy) */
    uint8_t status = 255;
    for (int i = 0; i < 20; i++) {
        if (!i2c_read_reg8(VL53L0X_ADDR, VL53L0X_REG_RESULT_RANGE_STATUS, &status)) {
            return false;
        }
        if (status != 255) {
            break;
        }
        nrf_delay_ms(5);
    }

    /* Read 16-bit range value (0x1E=low, 0x1F=high).
     * Without full calibration, raw values are ~64x too large; scale to mm. */
    if (!i2c_read_bytes(VL53L0X_ADDR, VL53L0X_REG_RESULT_RANGE_VALUE, buf, 2)) {
        return false;
    }

    uint16_t raw = (uint16_t)((buf[1] << 8) | buf[0]);
    *distance_mm = raw / 64;
    return true;
}