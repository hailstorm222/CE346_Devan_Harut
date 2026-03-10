#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nrf_delay.h"
#include "microbit_v2.h"

#include "hw_config.h"
#include "i2c_simple.h"
#include "oled_64x48.h"
#include "vl53l0x_min.h"
#include "rotary_encoder.h"
#include "touch_sensor.h"

#define LINE_BUF_SIZE 24

int main(void) {
    printf("CE346 combined test: OLED, TOF, encoder, touch\n\n");

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

    /* Rotary encoder: P8 (A), P9 (B), P12 (button). */
    rotary_encoder_init();
    printf("Rotary encoder OK (P8=A, P9=B, P12=SW)\n");

    /* Capacitive touch: 4 pads on P1–P4 (see hw_config.h). */
    static const uint32_t touch_pins[NUM_TOUCH_PADS] = {
        TOUCH_PIN_0, TOUCH_PIN_1, TOUCH_PIN_2, TOUCH_PIN_3
    };
    touch_sensor_t touch[NUM_TOUCH_PADS];
    bool touch_states[NUM_TOUCH_PADS];
    touch_sensor_init_array(touch, touch_pins, NUM_TOUCH_PADS);
    printf("Touch pads OK (P1–P4)\n");

    /* Main loop: poll encoder + touch, read TOF, show all on OLED and serial */
    uint16_t distance_mm = 0;
    char line[LINE_BUF_SIZE];

    while (1) {
        rotary_encoder_poll();
        (void)touch_sensor_read_array(touch, touch_states, NUM_TOUCH_PADS);

        uint8_t pos = rotary_encoder_get_position();
        bool btn = rotary_encoder_button_pressed();

        if (vl53l0x_read_distance_mm(&distance_mm)) {
            printf("TOF:%u mm Pos:%u %s T:%d%d%d%d\n",
                   (unsigned)distance_mm, (unsigned)pos, btn ? "SEL" : "-",
                   touch_states[0] ? 1 : 0, touch_states[1] ? 1 : 0,
                   touch_states[2] ? 1 : 0, touch_states[3] ? 1 : 0);

            if (oled_ok) {
                oled_clear();
                oled_draw_string(0, 0, "TOF");
                snprintf(line, sizeof(line), "%u mm", distance_mm);
                oled_draw_string(0, 1, line);
                snprintf(line, sizeof(line), "Pos %u %s", (unsigned)pos, btn ? "[SEL]" : "");
                oled_draw_string(0, 2, line);
                snprintf(line, sizeof(line), "T:%c%c%c%c",
                        touch_states[0] ? '1' : '-', touch_states[1] ? '2' : '-',
                        touch_states[2] ? '3' : '-', touch_states[3] ? '4' : '-');
                oled_draw_string(0, 3, line);
                oled_display();
            }
        } else {
            printf("TOF fail Pos:%u %s T:%d%d%d%d\n",
                   (unsigned)pos, btn ? "SEL" : "-",
                   touch_states[0] ? 1 : 0, touch_states[1] ? 1 : 0,
                   touch_states[2] ? 1 : 0, touch_states[3] ? 1 : 0);
            if (oled_ok) {
                oled_clear();
                oled_draw_string(0, 0, "TOF FAIL");
                snprintf(line, sizeof(line), "Pos %u %s", (unsigned)pos, btn ? "[SEL]" : "");
                oled_draw_string(0, 1, line);
                snprintf(line, sizeof(line), "T:%c%c%c%c",
                        touch_states[0] ? '1' : '-', touch_states[1] ? '2' : '-',
                        touch_states[2] ? '3' : '-', touch_states[3] ? '4' : '-');
                oled_draw_string(0, 2, line);
                oled_display();
            }
        }

        nrf_delay_ms(100);
    }
}
