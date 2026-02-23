#include "mb_hal.h"

#include <stdint.h>

#if defined(MB_USE_ESP_IDF)

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_timer.h"

int mb_hal_gpio_write(uint8_t pin, uint8_t level) {
    esp_err_t err = gpio_set_level((gpio_num_t)pin, (uint32_t)(level ? 1 : 0));
    return err == ESP_OK ? 0 : -1;
}

int mb_hal_gpio_read(uint8_t pin, uint8_t *out_level) {
    if (out_level == NULL) {
        return -1;
    }
    *out_level = (uint8_t)gpio_get_level((gpio_num_t)pin);
    return 0;
}

int mb_hal_pwm_set_duty(uint8_t channel, uint16_t permille) {
    uint32_t duty;
    ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel_t ledc_channel = (ledc_channel_t)channel;

    if (permille > 1000) {
        return -1;
    }

    duty = (permille * 8191U) / 1000U;

    if (ledc_set_duty(speed_mode, ledc_channel, duty) != ESP_OK) {
        return -1;
    }
    if (ledc_update_duty(speed_mode, ledc_channel) != ESP_OK) {
        return -1;
    }
    return 0;
}

int mb_hal_pwm_config(uint8_t channel, uint32_t frequency_hz) {
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = (ledc_timer_t)channel,
        .freq_hz = frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    return ledc_timer_config(&timer_cfg) == ESP_OK ? 0 : -1;
}

int mb_hal_i2c_read_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *out_value) {
    i2c_master_bus_handle_t bus_handle = (i2c_master_bus_handle_t)(uintptr_t)bus;
    i2c_master_dev_handle_t dev_handle = NULL;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t err;

    if (out_value == NULL) {
        return -1;
    }

    err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        return -1;
    }

    err = i2c_master_transmit_receive(dev_handle, &reg, 1, out_value, 1, 20);
    (void)i2c_master_bus_rm_device(dev_handle);

    return err == ESP_OK ? 0 : -1;
}

int mb_hal_i2c_write_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t value) {
    i2c_master_bus_handle_t bus_handle = (i2c_master_bus_handle_t)(uintptr_t)bus;
    i2c_master_dev_handle_t dev_handle = NULL;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    uint8_t tx[2] = {reg, value};
    esp_err_t err;

    err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        return -1;
    }

    err = i2c_master_transmit(dev_handle, tx, sizeof(tx), 20);
    (void)i2c_master_bus_rm_device(dev_handle);
    return err == ESP_OK ? 0 : -1;
}

uint32_t mb_hal_monotonic_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

void mb_hal_delay_ms(uint32_t delay_ms) {
    uint64_t start = esp_timer_get_time();
    uint64_t wait_us = (uint64_t)delay_ms * 1000ULL;
    while ((esp_timer_get_time() - start) < wait_us) {
    }
}

#else

int mb_hal_gpio_write(uint8_t pin, uint8_t level) {
    (void)pin;
    (void)level;
    return -1;
}

int mb_hal_gpio_read(uint8_t pin, uint8_t *out_level) {
    (void)pin;
    if (out_level != NULL) {
        *out_level = 0;
    }
    return -1;
}

int mb_hal_pwm_set_duty(uint8_t channel, uint16_t permille) {
    (void)channel;
    (void)permille;
    return -1;
}

int mb_hal_pwm_config(uint8_t channel, uint32_t frequency_hz) {
    (void)channel;
    (void)frequency_hz;
    return -1;
}

int mb_hal_i2c_read_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *out_value) {
    (void)bus;
    (void)addr;
    (void)reg;
    if (out_value != NULL) {
        *out_value = 0;
    }
    return -1;
}

int mb_hal_i2c_write_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t value) {
    (void)bus;
    (void)addr;
    (void)reg;
    (void)value;
    return -1;
}

uint32_t mb_hal_monotonic_ms(void) {
    return 0;
}

void mb_hal_delay_ms(uint32_t delay_ms) {
    (void)delay_ms;
}

#endif
