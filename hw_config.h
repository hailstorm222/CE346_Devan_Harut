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
 * WIRING (try both; one side is encoder, the other is button + power):
 *
 * Option A - 3-pin side = encoder, 2-pin = button + GND:
 *   Encoder:  GND -> micro:bit GND,  CLK (or A) -> ENC_PIN_A (P8),  DT (or B) -> ENC_PIN_B (P9)
 *   Button:   one pin -> ENC_PIN_SW (P12),  other pin -> GND  (button shorts to GND when pressed)
 *
 * Option B - 2-pin side = encoder, 3-pin = VCC/GND/SW:
 *   Encoder:  A/CLK -> ENC_PIN_A (P8),  B/DT -> ENC_PIN_B (P9)
 *   Button:   SW -> ENC_PIN_SW (P12),  GND -> GND.  (VCC -> 3V if needed)
 *
 * Use internal pull-ups; encoder and button are active-low (pull to GND).
 */
#define ENC_PIN_A   EDGE_P8   /* Phase A / CLK */
#define ENC_PIN_B   EDGE_P9   /* Phase B / DT  */
#define ENC_PIN_SW  EDGE_P12  /* Push button (active-low) */