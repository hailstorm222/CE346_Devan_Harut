#pragma once

#include <stdbool.h>
#include <stdint.h>

bool i2c_init_bus(void);
bool i2c_probe(uint8_t addr7);
bool i2c_write_bytes(uint8_t addr7, const uint8_t* data, uint32_t len);
bool i2c_write_reg8(uint8_t addr7, uint8_t reg, uint8_t value);
bool i2c_read_reg8(uint8_t addr7, uint8_t reg, uint8_t* value);
bool i2c_read_bytes(uint8_t addr7, uint8_t reg, uint8_t* buf, uint32_t len);