#include "rotary_encoder.h"
#include "hw_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "nrf_gpio.h"
#include "nrfx_gpiote.h"

/* 24 detents: 4 quadrature steps per detent is common; use 2 if position doubles. */
#define ENC_STEPS_PER_DETENT 4
#define ENC_NUM_POSITIONS    24

/* Quadrature state: 2 bits (A, B). CW sequence 0->1->2->3->0; CCW 0->3->2->1->0. */
static const int8_t quad_table[] = {
    0, -1,  1,  0,  1,  0,  0, -1, -1,  0,  0,  1,  0,  1, -1,  0
};

/* Updated in ISR; main reads enc_raw for position. */
static volatile int32_t enc_raw;
static uint8_t enc_prev;       /* ISR only */
static int32_t zero_offset;   /* detent index when physical "top" = 0 */

static bool btn_curr;
static bool btn_prev;
static bool btn_was_pressed;

static void encoder_pin_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    (void)action;
    (void)pin;
    uint8_t a = nrf_gpio_pin_read(ENC_PIN_A) ? 1u : 0u;
    uint8_t b = nrf_gpio_pin_read(ENC_PIN_B) ? 1u : 0u;
    uint8_t curr = (a << 1) | b;
    unsigned idx = (enc_prev << 2) | curr;
    enc_prev = curr;
    enc_raw += (int32_t)quad_table[idx & 0xF];
}

void rotary_encoder_init(void) {
    enc_prev = (uint8_t)((nrf_gpio_pin_read(ENC_PIN_A) << 1) | nrf_gpio_pin_read(ENC_PIN_B));
    enc_raw = 0;
    zero_offset = 0;

    /* GPIOTE: both encoder pins trigger on any edge (toggle), pull-up. */
    if (!nrfx_gpiote_is_init()) {
        nrfx_gpiote_init();
    }
    nrfx_gpiote_in_config_t enc_cfg = {
        .sense = NRF_GPIOTE_POLARITY_TOGGLE,
        .pull = NRF_GPIO_PIN_PULLUP,
        .is_watcher = false,
        .hi_accuracy = true,
        .skip_gpio_setup = false,
    };
    nrfx_gpiote_in_init(ENC_PIN_A, &enc_cfg, encoder_pin_handler);
    nrfx_gpiote_in_event_enable(ENC_PIN_A, true);
    nrfx_gpiote_in_init(ENC_PIN_B, &enc_cfg, encoder_pin_handler);
    nrfx_gpiote_in_event_enable(ENC_PIN_B, true);

    /* Button: polled, no interrupt */
    nrf_gpio_cfg_input(ENC_PIN_SW, NRF_GPIO_PIN_PULLUP);
    btn_curr = !nrf_gpio_pin_read(ENC_PIN_SW);
    btn_prev = btn_curr;
    btn_was_pressed = false;
}

void rotary_encoder_poll(void) {
    /* Only poll the button; encoder is interrupt-driven. */
    btn_curr = !nrf_gpio_pin_read(ENC_PIN_SW);
    btn_was_pressed = btn_curr && !btn_prev;
    btn_prev = btn_curr;
}

uint8_t rotary_encoder_get_position(void) {
    int32_t raw = (int32_t)enc_raw;
    int32_t detent = raw / ENC_STEPS_PER_DETENT;
    int32_t pos = (detent - zero_offset) % (int32_t)ENC_NUM_POSITIONS;
    if (pos < 0) pos += ENC_NUM_POSITIONS;
    return (uint8_t)(pos % ENC_NUM_POSITIONS);
}

void rotary_encoder_set_zero(void) {
    zero_offset = (int32_t)enc_raw / ENC_STEPS_PER_DETENT;
}

bool rotary_encoder_button_pressed(void) {
    return btn_curr;
}

bool rotary_encoder_button_was_pressed(void) {
    return btn_was_pressed;
}
