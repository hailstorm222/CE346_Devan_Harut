#pragma once

#include "microbit_v2.h"

#define QWIIC_SCL_PIN   I2C_QWIIC_SCL
#define QWIIC_SDA_PIN   I2C_QWIIC_SDA

#define OLED_ADDR_1     0x3D
#define OLED_ADDR_2     0x3C
#define VL53L0X_ADDR    0x29

/* Set to 0 to skip DataInit (0x89, 0x88). */
#define VL53L0X_USE_INIT 1

/*
 * Rotary encoder (5-pin: 2 on one side, 3 on the other). 24 detents.
 */
#define ENC_PIN_A   EDGE_P8   /* Phase A / CLK */
#define ENC_PIN_B   EDGE_P9   /* Phase B / DT  */
#define ENC_PIN_SW  EDGE_P12  /* Push button (active-low) */

/*
 * Capacitive touch: 4 pads on edge connector 
 */
#define TOUCH_PIN_0  EDGE_P1
#define TOUCH_PIN_1  EDGE_P2
#define TOUCH_PIN_2  EDGE_P3
#define TOUCH_PIN_3  EDGE_P4
#define NUM_TOUCH_PADS 4
