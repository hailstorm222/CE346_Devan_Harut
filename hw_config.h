#pragma once

#include "microbit_v2.h"

#define QWIIC_SCL_PIN   I2C_QWIIC_SCL
#define QWIIC_SDA_PIN   I2C_QWIIC_SDA

#define OLED_ADDR_1     0x3D
#define OLED_ADDR_2     0x3C
#define VL53L0X_ADDR    0x29

/* Set to 0 to skip DataInit (0x89, 0x88). */
#define VL53L0X_USE_INIT 1