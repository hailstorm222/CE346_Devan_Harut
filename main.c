#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nrf_delay.h"
#include "microbit_v2.h"

#include "hw_config.h"
#include "i2c_simple.h"

int main(void) {
    printf("I2C scan - checking for devices on Qwiic bus\n\n");

    if (!i2c_init_bus()) {
        printf("ERROR: I2C init failed\n");
        while (1) {
            nrf_delay_ms(1000);
        }
    }

    printf("Scanning addresses 0x08 to 0x77...\n");

    uint8_t count = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_probe(addr)) {
            printf("  [FOUND] 0x%02X\n", addr);
            count++;
        }
    }

    printf("\nDone. Found %u device(s).\n", count);
    if (count == 0) {
        printf("(Expected: OLED at 0x3C or 0x3D, VL53L0X at 0x29)\n");
    }

    while (1) {
        nrf_delay_ms(1000);
    }
}
