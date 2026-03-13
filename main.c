#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "nrf_delay.h"
#include "microbit_v2.h"

#include "control_protocol.h"
#include "hw_config.h"
#include "i2c_simple.h"
#include "oled_64x48.h"
#include "rotary_encoder.h"
#include "touch_sensor.h"
#include "vl53l0x_min.h"

#define LINE_BUF_SIZE 24
/* Poll period: touch and encoder button are polled here; encoder rotation is interrupt-driven (GPIOTE). */
#define MAIN_LOOP_MS 10
#define HEARTBEAT_INTERVAL_MS 1000
#define PAD_HOLD_MS 250
/* TOF: clamp to 50-400 mm for effect depth; smooth with 1/4 blend; re-report when change >= step. */
#define TOF_MIN_MM 50
#define TOF_MAX_MM 400
#define TOF_REPORT_STEP_MM 15
#define TOF_SMOOTH_SHIFT 2
/* Encoder slots: 4 effects + 1 slice-edit mode to match host. */
#define NUM_SLOTS 5

static const uint32_t touch_pins[NUM_TOUCH_PADS] = {
    TOUCH_PIN_0, TOUCH_PIN_1, TOUCH_PIN_2, TOUCH_PIN_3
};

static const char *slot_names[NUM_SLOTS] = {
    "FILTER",
    "DELAY",
    "REVERB",
    "BITCRSH",
    "SLICE",
};

static uint16_t clamp_tof_mm(uint16_t distance_mm) {
    if (distance_mm < TOF_MIN_MM) {
        return TOF_MIN_MM;
    }
    if (distance_mm > TOF_MAX_MM) {
        return TOF_MAX_MM;
    }
    return distance_mm;
}

static int8_t encoder_delta_from_position(uint8_t current, uint8_t previous) {
    int delta = (int)current - (int)previous;
    if (delta > 12) {
        delta -= 24;
    } else if (delta < -12) {
        delta += 24;
    }
    return (int8_t)delta;
}

static uint8_t wrap_slot_index(int slot_index) {
    while (slot_index < 0) {
        slot_index += NUM_SLOTS;
    }
    return (uint8_t)(slot_index % NUM_SLOTS);
}

static void update_oled_status(bool oled_ok,
                               bool tof_valid,
                               uint16_t tof_mm,
                               uint8_t slot_index,
                               bool slice_edit_mode,
                               bool effect_enabled,
                               bool encoder_button,
                               const bool *touch_states) {
    char line[LINE_BUF_SIZE];

    if (!oled_ok) {
        return;
    }

    oled_clear();
    /* Only show "SLICE EDIT" when actually in slice-edit (entered by button); else show slot. */
    if (slice_edit_mode) {
        snprintf(line, sizeof(line), "SLICE EDIT");
    } else if (slot_index == (NUM_SLOTS - 1)) {
        snprintf(line, sizeof(line), "%s (BTN)", slot_names[slot_index]);
    } else {
        snprintf(line, sizeof(line), "%s %s",
                 slot_names[slot_index],
                 effect_enabled ? "ON" : "OFF");
    }
    oled_draw_string(0, 0, line);

    if (tof_valid) {
        snprintf(line, sizeof(line), "TOF %u mm", (unsigned int)tof_mm);
    } else {
        snprintf(line, sizeof(line), "TOF NO DATA");
    }
    oled_draw_string(0, 1, line);

    snprintf(line, sizeof(line), "T:%c%c%c%c B:%u",
             touch_states[0] ? '1' : '-',
             touch_states[1] ? '2' : '-',
             touch_states[2] ? '3' : '-',
             touch_states[3] ? '4' : '-',
             encoder_button ? 1u : 0u);
    oled_draw_string(0, 2, line);

    oled_draw_string(0, 3, "USB CTRL READY");
    oled_display();
}

int main(void) {
    printf("CE346 air sampler controller\n");

    /* Init I2C on Qwiic bus (P19/P20) */
    if (!i2c_init_bus_with_pins(I2C_QWIIC_SCL, I2C_QWIIC_SDA)) {
        printf("ERROR I2C_INIT\n");
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
        printf("OLED_READY 0x%02X\n", oled_addr);
        oled_clear();
        oled_draw_string(0, 0, "OLED READY");
        oled_display();
    } else {
        printf("WARN OLED_INIT\n");
    }

    bool tof_available = vl53l0x_present();
    if (tof_available) {
        printf("TOF_READY 0x%02X\n", VL53L0X_ADDR);
    } else {
        printf("WARN TOF_NOT_FOUND\n");
    }

#if VL53L0X_USE_INIT
    if (tof_available && !vl53l0x_init()) {
        printf("WARN TOF_INIT\n");
    }
#endif

    if (tof_available) {
        uint8_t model = 0;
        uint8_t rev = 0;
        if (vl53l0x_read_id(&model, &rev)) {
            printf("TOF_ID model=0x%02X rev=0x%02X\n", model, rev);
        }
    }

    /* Rotary encoder: P8 (A), P9 (B), P12 (button). */
    rotary_encoder_init();
    printf("ENC_READY\n");

    /* Capacitive touch: 4 pads on P1–P4 (see hw_config.h). */
    touch_sensor_t touch[NUM_TOUCH_PADS];
    bool touch_states[NUM_TOUCH_PADS];
    bool touch_prev[NUM_TOUCH_PADS] = {false};
    bool touch_hold_sent[NUM_TOUCH_PADS] = {false};
    uint32_t touch_down_ms[NUM_TOUCH_PADS] = {0};
    touch_sensor_init_array(touch, touch_pins, NUM_TOUCH_PADS);
    printf("TOUCH_READY pads=%u\n", (unsigned int)NUM_TOUCH_PADS);

    uint8_t last_encoder_pos = rotary_encoder_get_position();
    bool last_encoder_button = rotary_encoder_button_pressed();
    uint8_t slot_index = 0;
    bool slice_edit_mode = false;
    bool effect_enabled = true;

    bool tof_valid = false;
    uint16_t tof_smoothed_mm = TOF_MAX_MM;
    uint16_t last_reported_tof_mm = 0;

    uint32_t elapsed_ms = 0;
    uint32_t last_heartbeat_ms = 0;

    while (1) {
        /* Encoder rotation is handled by GPIOTE ISR; we only poll the encoder button and touch here. */
        rotary_encoder_poll();
        (void)touch_sensor_read_array(touch, touch_states, NUM_TOUCH_PADS);

        for (uint8_t i = 0; i < NUM_TOUCH_PADS; ++i) {
            bool pressed = touch_states[i];

            if (pressed && !touch_prev[i]) {
                touch_down_ms[i] = elapsed_ms;
                touch_hold_sent[i] = false;
                control_protocol_emit_pad_down(i);
            } else if (!pressed && touch_prev[i]) {
                touch_hold_sent[i] = false;
                control_protocol_emit_pad_up(i);
            } else if (pressed && !touch_hold_sent[i] &&
                       ((elapsed_ms - touch_down_ms[i]) >= PAD_HOLD_MS)) {
                touch_hold_sent[i] = true;
                control_protocol_emit_pad_hold(i);
            }
            touch_prev[i] = pressed;
        }

        uint8_t encoder_pos = rotary_encoder_get_position();
        int8_t encoder_delta = encoder_delta_from_position(encoder_pos, last_encoder_pos);
        if (encoder_delta != 0) {
            control_protocol_emit_encoder_delta(encoder_delta);
            /* In slice-edit mode, encoder moves slice start; do not change displayed slot. */
            if (!slice_edit_mode) {
                slot_index = wrap_slot_index((int)slot_index + (int)encoder_delta);
            }
            last_encoder_pos = encoder_pos;
        }

        bool encoder_button = rotary_encoder_button_pressed();
        if (encoder_button != last_encoder_button) {
            control_protocol_emit_encoder_button(encoder_button);
            if (encoder_button) {
                if (slot_index == (NUM_SLOTS - 1)) {
                    /* SLICE slot: button toggles slice-edit mode (enter/exit). */
                    slice_edit_mode = !slice_edit_mode;
                } else if (!slice_edit_mode) {
                    effect_enabled = !effect_enabled;
                }
            }
            last_encoder_button = encoder_button;
        }

        if (tof_available) {
            uint16_t raw_distance_mm = 0;
            if (vl53l0x_read_distance_mm(&raw_distance_mm)) {
                uint16_t clamped = clamp_tof_mm(raw_distance_mm);
                if (!tof_valid) {
                    tof_smoothed_mm = clamped;
                } else {
                    int32_t diff = (int32_t)clamped - (int32_t)tof_smoothed_mm;
                    tof_smoothed_mm = (uint16_t)((int32_t)tof_smoothed_mm +
                                                 (diff >> TOF_SMOOTH_SHIFT));
                }

                if (!tof_valid ||
                    (abs((int)tof_smoothed_mm - (int)last_reported_tof_mm) >= TOF_REPORT_STEP_MM)) {
                    control_protocol_emit_tof(tof_smoothed_mm);
                    last_reported_tof_mm = tof_smoothed_mm;
                }

                tof_valid = true;
            }
        }

        update_oled_status(oled_ok,
                           tof_valid,
                           tof_smoothed_mm,
                           slot_index,
                           slice_edit_mode,
                           effect_enabled,
                           encoder_button,
                           touch_states);

        if ((elapsed_ms - last_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS) {
            control_protocol_emit_heartbeat(tof_smoothed_mm,
                                            tof_valid,
                                            slot_index,
                                            effect_enabled,
                                            touch_states,
                                            NUM_TOUCH_PADS);
            last_heartbeat_ms = elapsed_ms;
        }

        nrf_delay_ms(MAIN_LOOP_MS);
        elapsed_ms += MAIN_LOOP_MS;
    }
}
