#include "i2c_simple.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "app_error.h"
#include "nrf_drv_twi.h"
#include "sdk_errors.h"
#include "hw_config.h"

static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);
static bool m_initialized = false;

static bool i2c_init_impl(uint32_t scl_pin, uint32_t sda_pin) {
    const nrf_drv_twi_config_t config = {
        .scl                = scl_pin,
        .sda                = sda_pin,
        .frequency          = NRF_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_LOW,
        .clear_bus_init     = true
    };

    ret_code_t err = nrf_drv_twi_init(&m_twi, &config, NULL, NULL);
    if (err != NRF_SUCCESS) {
        printf("TWI init failed: %lu\n", (unsigned long)err);
        return false;
    }

    nrf_drv_twi_enable(&m_twi);
    m_initialized = true;
    return true;
}

bool i2c_init_bus(void) {
    if (m_initialized) {
        return true;
    }
    bool ok = i2c_init_impl(QWIIC_SCL_PIN, QWIIC_SDA_PIN);
    if (ok) {
        printf("TWI init OK (Qwiic: P19/P20)\n");
    }
    return ok;
}

void i2c_deinit_bus(void) {
    if (!m_initialized) {
        return;
    }
    nrf_drv_twi_disable(&m_twi);
    nrf_drv_twi_uninit(&m_twi);
    m_initialized = false;
}

bool i2c_init_bus_with_pins(uint32_t scl_pin, uint32_t sda_pin) {
    if (m_initialized) {
        i2c_deinit_bus();
    }
    return i2c_init_impl(scl_pin, sda_pin);
}

bool i2c_probe(uint8_t addr7) {
    /* TWIM/EasyDMA hangs with length=0. Send 1 byte and check error code:
     * - NRF_SUCCESS or DNACK = device exists (ACKed address)
     * - ANACK = no device at this address */
    static uint8_t dummy = 0x00;
    ret_code_t err = nrf_drv_twi_tx(&m_twi, addr7, &dummy, 1, false);

    if (err == NRF_SUCCESS) {
        return true;
    }
    if (err == NRF_ERROR_DRV_TWI_ERR_DNACK) {
        return true;
    }
    return false;
}

bool i2c_write_bytes(uint8_t addr7, const uint8_t* data, uint32_t len) {
    ret_code_t err = nrf_drv_twi_tx(&m_twi, addr7, data, len, false);
    return (err == NRF_SUCCESS);
}

bool i2c_write_reg8(uint8_t addr7, uint8_t reg, uint8_t value) {
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;
    return i2c_write_bytes(addr7, buf, 2);
}

bool i2c_read_reg8(uint8_t addr7, uint8_t reg, uint8_t* value) {
    ret_code_t err;

    err = nrf_drv_twi_tx(&m_twi, addr7, &reg, 1, true);
    if (err != NRF_SUCCESS) {
        return false;
    }

    err = nrf_drv_twi_rx(&m_twi, addr7, value, 1);
    return (err == NRF_SUCCESS);
}

bool i2c_read_bytes(uint8_t addr7, uint8_t reg, uint8_t* buf, uint32_t len) {
    ret_code_t err;

    err = nrf_drv_twi_tx(&m_twi, addr7, &reg, 1, true);
    if (err != NRF_SUCCESS) {
        return false;
    }

    err = nrf_drv_twi_rx(&m_twi, addr7, buf, len);
    return (err == NRF_SUCCESS);
}