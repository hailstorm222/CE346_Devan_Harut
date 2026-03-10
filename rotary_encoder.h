#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Rotary encoder with 24 detents (0–23), top = 0, wrap. Optional push button (select).
 * Encoder A/B are interrupt-driven (GPIOTE, both edges); position updates on every turn.
 * Button is polled in rotary_encoder_poll(). */

void rotary_encoder_init(void);

/* Call regularly. Only updates button state; encoder position is interrupt-driven. */
void rotary_encoder_poll(void);

/* Current position 0–23 (top = 0, wraps after 23). */
uint8_t rotary_encoder_get_position(void);

/* Call when knob is at physical "top" so that position reports 0. */
void rotary_encoder_set_zero(void);

/* True if button is currently pressed (debounced). Use as select. */
bool rotary_encoder_button_pressed(void);

/* True on the poll when button just went from released to pressed (one shot). */
bool rotary_encoder_button_was_pressed(void);
