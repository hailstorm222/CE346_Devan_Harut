#include "vl53l0x_min.h"

#include "i2c_simple.h"
#include "hw_config.h"

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