#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Display state from host (DISP lines). When no host updates received, use local state. */
#define DISPLAY_HOST_LINE_MAX 64

/* State from host; valid when host_updated is true. */
typedef struct {
    bool host_updated;
    uint8_t slot;
    bool slice_edit;
    uint8_t edit_pad;
    bool effect_on[4];
    uint8_t effect_depth[4]; /* 0-100 */
    bool playing[4];
} display_host_state_t;

void display_host_init(void);
void display_host_rx_byte(uint8_t byte);
void display_host_poll(void);
void display_host_get_state(display_host_state_t *out);
