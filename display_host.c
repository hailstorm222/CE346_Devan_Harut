/*
 * Receives DISP lines from host over UART and parses into display_host_state_t.
 * The board UART handler calls display_host_rx_done(byte); we implement it here
 * so RX bytes reach the parser. Call display_host_poll() in main loop.
 */
#include "display_host.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nrf_drv_uart.h"

extern nrf_drv_uart_t m_uart;

#define RX_LINE_BUF_SIZE 80

static display_host_state_t s_state;
static char s_line_buf[RX_LINE_BUF_SIZE];
static uint16_t s_line_len;
static uint8_t s_rx_byte;
static bool s_rx_started;

static void parse_disp_line(void) {
    if (s_line_len < 5 || memcmp(s_line_buf, "DISP ", 5) != 0) {
        return;
    }
    char *p = s_line_buf + 5;
    s_state.host_updated = true;

    while (*p) {
        if (p[0] == 's' && p[1] == '=') {
            p += 2;
            int n = 0;
            while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
            if (n >= 0 && n <= 4) s_state.slot = (uint8_t)n;
        } else if (p[0] == 'e' && p[1] == '=') {
            p += 2;
            s_state.slice_edit = (*p == '1');
            p++;
        } else if (p[0] == 'p' && p[1] == '=') {
            p += 2;
            int n = 0;
            while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
            if (n >= 0 && n <= 3) s_state.edit_pad = (uint8_t)n;
        } else if (p[0] == 'v' && p[1] == '=') {
            p += 2;
            for (int i = 0; i < 4 && *p; i++) {
                s_state.effect_on[i] = (*p == '1');
                p++;
            }
        } else if (p[0] == 'd' && p[1] == '=') {
            p += 2;
            for (int i = 0; i < 4 && *p; i++) {
                int n = 0;
                while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
                if (n > 100) n = 100;
                s_state.effect_depth[i] = (uint8_t)n;
                if (*p == ',') p++;
            }
        } else if (p[0] == 'P' && p[1] == '=') {
            p += 2;
            for (int i = 0; i < 4 && *p; i++) {
                s_state.playing[i] = (*p == '1');
                p++;
            }
        }
        while (*p && *p != ' ') p++;
        if (*p == ' ') p++;
    }
}

void display_host_init(void) {
    memset(&s_state, 0, sizeof(s_state));
    s_line_len = 0;
    s_rx_started = false;
    if (nrf_drv_uart_rx(&m_uart, &s_rx_byte, 1) == NRF_SUCCESS) {
        s_rx_started = true;
    }
}

/* Strong definition: board calls this on UART RX_DONE so host DISP lines are received. */
void display_host_rx_done(uint8_t byte) {
    display_host_rx_byte(byte);
}

void display_host_rx_byte(uint8_t byte) {
    if (byte == '\n' || byte == '\r') {
        if (s_line_len > 0) {
            s_line_buf[s_line_len] = '\0';
            parse_disp_line();
        }
        s_line_len = 0;
    } else if (s_line_len < RX_LINE_BUF_SIZE - 1 && byte >= ' ') {
        s_line_buf[s_line_len++] = (char)byte;
    }
    /* Always start next RX so we keep receiving (critical: was missing after newline). */
    (void)nrf_drv_uart_rx(&m_uart, &s_rx_byte, 1);
}

void display_host_poll(void) {
    (void)s_rx_started;
}

void display_host_get_state(display_host_state_t *out) {
    if (out) {
        memcpy(out, &s_state, sizeof(display_host_state_t));
    }
}
