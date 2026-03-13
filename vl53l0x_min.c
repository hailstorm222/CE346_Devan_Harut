/*
 * VL53L0X minimal driver: presence, optional init, ID, single-shot distance in mm.
 * Result format matches ST API: read 12 bytes from 0x14; range in mm at bytes 10-11
 * (registers 0x1E = MSB, 0x1F = LSB), i.e. raw = (buf[10]<<8)|buf[11].
 * 8190 (0x1FFE) = no target / out of range; we report 2000 mm then main clamps to 50-400.
 */
#include "vl53l0x_min.h"

#include <stdbool.h>
#include <stdint.h>

#include "i2c_simple.h"
#include "hw_config.h"
#include "nrf_delay.h"

/* ST API register addresses (from vl53l0x_api) */
#define VL53L0X_REG_SYSRANGE_START               0x00
#define VL53L0X_REG_RESULT_RANGE_STATUS         0x14
#define VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV  0x89
#define VL53L0X_REG_SYSTEM_RANGE_CONFIG         0x09

bool vl53l0x_present(void) {
    return i2c_probe(VL53L0X_ADDR);
}

/* Minimal DataInit from ST API: 2.8V I2C (bit 0 of 0x89), I2C standard mode (0x88) */
bool vl53l0x_init(void) {
    uint8_t val;

    /* Set 2.8V I2C: UpdateByte(0x89, 0xFE, 0x01) -> set bit 0 */
    if (!i2c_read_reg8(VL53L0X_ADDR, VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, &val)) {
        return false;
    }
    val = (val & 0xFE) | 0x01;
    if (!i2c_write_reg8(VL53L0X_ADDR, VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, val)) {
        return false;
    }

    /* I2C standard mode */
    if (!i2c_write_reg8(VL53L0X_ADDR, 0x88, 0x00)) {
        return false;
    }

    return true;
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
    uint8_t buf[12];

    /* Start single-shot ranging */
    if (!i2c_write_reg8(VL53L0X_ADDR, VL53L0X_REG_SYSRANGE_START, 0x01)) {
        return false;
    }

    /* Wait for measurement (typically 20-30 ms) */
    nrf_delay_ms(30);

    /* Poll until done: status 255 = no update (busy), any other = has data */
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
    if (status == 255) {
        return false;  /* Timeout waiting for data */
    }

    /* Read 12 bytes from 0x14 (ST API layout) */
    if (!i2c_read_bytes(VL53L0X_ADDR, VL53L0X_REG_RESULT_RANGE_STATUS, buf, 12)) {
        return false;
    }

    /* Range at bytes 10-11 (registers 0x1E-0x1F). ST API: MAKEUINT16(localBuffer[11], localBuffer[10])
     * => (buf[10]<<8)|buf[11] (MSB first). Do not swap; swapped gives 20 mm as 5120. */
    uint16_t raw = (uint16_t)((buf[10] << 8) | buf[11]);

    /* 8190 (0x1FFE) = no target. Main clamps to TOF_MIN_MM..TOF_MAX_MM (50-400) for effect depth. */
    if (raw >= 8190) {
        *distance_mm = 2000;
    } else {
        *distance_mm = raw;
    }
    return true;
}