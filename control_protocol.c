#include "control_protocol.h"

#include <stdio.h>

static void protocol_flush(void) {
    fflush(stdout);
}

void control_protocol_emit_pad_down(uint8_t pad_id) {
    printf("PAD_DOWN %u\n", (unsigned int)pad_id);
    protocol_flush();
}

void control_protocol_emit_pad_up(uint8_t pad_id) {
    printf("PAD_UP %u\n", (unsigned int)pad_id);
    protocol_flush();
}

void control_protocol_emit_pad_hold(uint8_t pad_id) {
    printf("PAD_HOLD %u\n", (unsigned int)pad_id);
    protocol_flush();
}

void control_protocol_emit_encoder_delta(int8_t delta) {
    printf("ENC_DELTA %d\n", (int)delta);
    protocol_flush();
}

void control_protocol_emit_encoder_button(bool pressed) {
    printf("ENC_BTN %u\n", pressed ? 1u : 0u);
    protocol_flush();
}

void control_protocol_emit_tof(uint16_t distance_mm) {
    printf("TOF %u\n", (unsigned int)distance_mm);
    protocol_flush();
}

void control_protocol_emit_heartbeat(uint16_t tof_mm,
                                     bool tof_valid,
                                     uint8_t effect_index,
                                     bool effect_enabled,
                                     const bool *touch_states,
                                     size_t touch_count) {
    printf("HEARTBEAT effect=%u enabled=%u tof=",
           (unsigned int)effect_index,
           effect_enabled ? 1u : 0u);

    if (tof_valid) {
        printf("%u", (unsigned int)tof_mm);
    } else {
        printf("NA");
    }

    printf(" touch=");
    for (size_t i = 0; i < touch_count; ++i) {
        putchar(touch_states[i] ? '1' : '0');
    }
    putchar('\n');
    protocol_flush();
}
