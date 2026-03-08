#pragma once

#include <stdbool.h>
#include <stdint.h>

bool vl53l0x_present(void);
bool vl53l0x_read_id(uint8_t* model, uint8_t* revision);