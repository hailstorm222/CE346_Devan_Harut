#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nrf_delay.h"
#include "microbit_v2.h"

#include "hw_config.h"
#include "i2c_simple.h"
#include "oled_64x48.h"
#include "vl53l0x_min.h"

static void scan_i2c_bus(void) {
    printf("I2C scan start\n");

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_probe(addr)) {
            printf("  found device at 0x%02X\n", addr);
        }
    }

    printf("I2C scan done\n");
}

int main(void) {
    printf("micro:bit V2 Qwiic bring-up starting\n");

    if (!i2c_init_bus()) {
        printf("ERROR: I2C init failed\n");
        while (1) {
            nrf_delay_ms(1000);
        }
    }

    scan_i2c_bus();

    bool oled_ok = false;
    uint8_t oled_addr = 0;

    if (i2c_probe(OLED_ADDR_1)) {
        oled_addr = OLED_ADDR_1;
        oled_ok = oled_begin(oled_addr);
    } else if (i2c_probe(OLED_ADDR_2)) {
        oled_addr = OLED_ADDR_2;
        oled_ok = oled_begin(oled_addr);
    }

    if (oled_ok) {
        printf("OLED found at 0x%02X\n", oled_addr);
        oled_clear();
        oled_draw_string(0, 0, "OLED OK");
        oled_draw_string(0, 1, "I2C READY");
        oled_display();
    } else {
        printf("OLED not found\n");
    }

    if (vl53l0x_present()) {
        uint8_t model = 0;
        uint8_t rev = 0;
        printf("VL53L0X found at 0x%02X\n", VL53L0X_ADDR);

        if (vl53l0x_read_id(&model, &rev)) {
            printf("VL53L0X ID model=0x%02X rev=0x%02X\n", model, rev);

            if (oled_ok) {
                char line1[20];
                char line2[20];

                snprintf(line1, sizeof(line1), "VL53 OK");
                snprintf(line2, sizeof(line2), "ID %02X %02X", model, rev);

                oled_clear();
                oled_draw_string(0, 0, "OLED OK");
                oled_draw_string(0, 1, line1);
                oled_draw_string(0, 2, line2);
                oled_display();
            }
        } else {
            printf("VL53L0X present but ID read failed\n");

            if (oled_ok) {
                oled_clear();
                oled_draw_string(0, 0, "OLED OK");
                oled_draw_string(0, 1, "VL53 FOUND");
                oled_draw_string(0, 2, "ID FAIL");
                oled_display();
            }
        }
    } else {
        printf("VL53L0X not found\n");

        if (oled_ok) {
            oled_clear();
            oled_draw_string(0, 0, "OLED OK");
            oled_draw_string(0, 1, "VL53 NOT");
            oled_draw_string(0, 2, "FOUND");
            oled_display();
        }
    }

    while (1) {
        nrf_delay_ms(1000);
    }
}