#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void control_protocol_emit_pad_down(uint8_t pad_id);
void control_protocol_emit_pad_up(uint8_t pad_id);
void control_protocol_emit_pad_hold(uint8_t pad_id);
void control_protocol_emit_encoder_delta(int8_t delta);
void control_protocol_emit_encoder_button(bool pressed);
void control_protocol_emit_tof(uint16_t distance_mm);
void control_protocol_emit_heartbeat(uint16_t tof_mm,
                                     bool tof_valid,
                                     uint8_t effect_index,
                                     bool effect_enabled,
                                     const bool *touch_states,
                                     size_t touch_count);
