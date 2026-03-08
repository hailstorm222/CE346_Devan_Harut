#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nrf_delay.h"
#include "microbit_v2.h"

#include "hw_config.h"
#include "i2c_simple.h"
#include "oled_64x48.h"
#include "vl53l0x_min.h"

#define LINE_BUF_SIZE 16

int main(void) {
    printf("CE346 OLED + TOF test\n\n");

    /* Init I2C on Qwiic bus (P19/P20) */
    if (!i2c_init_bus_with_pins(I2C_QWIIC_SCL, I2C_QWIIC_SDA)) {
        printf("ERROR: I2C init failed\n");
        while (1) { nrf_delay_ms(1000); }
    }

    /* Init OLED - try 0x3D first (your scan found it), then 0x3C */
    bool oled_ok = false;
    uint8_t oled_addr = 0;

    if (i2c_probe(OLED_ADDR_1)) {
        oled_addr = OLED_ADDR_1;
        oled_ok = oled_begin(oled_addr);
    }
    if (!oled_ok && i2c_probe(OLED_ADDR_2)) {
        oled_addr = OLED_ADDR_2;
        oled_ok = oled_begin(oled_addr);
    }

    if (oled_ok) {
        printf("OLED OK at 0x%02X\n", oled_addr);
        oled_clear();
        oled_draw_string(0, 0, "OLED READY");
        oled_display();
    } else {
        printf("OLED init failed\n");
    }

    /* Check VL53L0X */
    if (!vl53l0x_present()) {
        printf("VL53L0X not found\n");
        if (oled_ok) {
            oled_clear();
            oled_draw_string(0, 0, "TOF NOT");
            oled_draw_string(0, 1, "FOUND");
            oled_display();
        }
        while (1) { nrf_delay_ms(1000); }
    }

    printf("VL53L0X OK at 0x%02X\n", VL53L0X_ADDR);

#if VL53L0X_USE_INIT
    if (!vl53l0x_init()) {
        printf("VL53L0X init failed (continuing anyway)\n");
    }
#endif

    uint8_t model = 0, rev = 0;
    if (vl53l0x_read_id(&model, &rev)) {
        printf("VL53L0X ID: model=0x%02X rev=0x%02X\n", model, rev);
    }

    /* Main loop: read TOF distance, display on OLED and serial */
    uint16_t distance_mm = 0;
    char line[LINE_BUF_SIZE];

    while (1) {
        if (vl53l0x_read_distance_mm(&distance_mm)) {
            printf("Distance: %u mm\n", distance_mm);

            if (oled_ok) {
                oled_clear();
                oled_draw_string(0, 0, "TOF SENSOR");
                snprintf(line, sizeof(line), "%u mm", distance_mm);
                oled_draw_string(0, 1, line);
                oled_display();
            }
        } else {
            printf("TOF read failed\n");
            if (oled_ok) {
                oled_clear();
                oled_draw_string(0, 0, "TOF READ");
                oled_draw_string(0, 1, "FAIL");
                oled_display();
            }
        }

        nrf_delay_ms(200);
    }
}
