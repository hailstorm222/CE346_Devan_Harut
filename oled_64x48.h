#pragma once

#include <stdbool.h>
#include <stdint.h>

bool oled_begin(uint8_t addr7);
void oled_clear(void);
void oled_draw_string(uint8_t x, uint8_t page, const char* s);
bool oled_display(void);
uint8_t oled_get_addr(void);