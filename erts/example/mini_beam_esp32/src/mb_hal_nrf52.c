#include "mb_hal.h"

#include <stdint.h>

#if defined(MB_USE_NRF52)

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
static const struct gpio_dt_spec mb_gpio0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#define MB_HAS_LED0 1
#else
#define MB_HAS_LED0 0
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm0), okay)
static const struct device *mb_pwm0 = DEVICE_DT_GET(DT_NODELABEL(pwm0));
#define MB_HAS_PWM0 1
#else
#define MB_HAS_PWM0 0
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(i2c0), okay)
#define MB_I2C0_NODE DT_ALIAS(i2c0)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
#define MB_I2C0_NODE DT_NODELABEL(i2c0)
#endif

#ifdef MB_I2C0_NODE
static const struct device *mb_i2c0 = DEVICE_DT_GET(MB_I2C0_NODE);
#define MB_HAS_I2C0 1
#else
#define MB_HAS_I2C0 0
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
static const struct device *mb_i2c1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));
#define MB_HAS_I2C1 1
#else
#define MB_HAS_I2C1 0
#endif

#if defined(MB_STRICT_DTS)
#if !MB_HAS_LED0
#error "MB_STRICT_DTS requires devicetree alias led0"
#endif
#if !MB_HAS_PWM0
#error "MB_STRICT_DTS requires devicetree node label pwm0"
#endif
#if !MB_HAS_I2C0
#error "MB_STRICT_DTS requires devicetree alias i2c0 or node label i2c0"
#endif
#endif

static const struct device *mb_get_i2c_bus(uint8_t bus) {
    if (bus == 0U) {
#if MB_HAS_I2C0
        if (device_is_ready(mb_i2c0)) {
            return mb_i2c0;
        }
#endif
        return NULL;
    }
    if (bus == 1U) {
#if MB_HAS_I2C1
        if (device_is_ready(mb_i2c1)) {
            return mb_i2c1;
        }
#endif
        return NULL;
    }
    return NULL;
}

int mb_hal_gpio_write(uint8_t pin, uint8_t level) {
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
    if (pin != 0 || !device_is_ready(mb_gpio0.port)) {
        return -1;
    }
    if (gpio_pin_configure_dt(&mb_gpio0, GPIO_OUTPUT_INACTIVE) != 0) {
        return -1;
    }
    return gpio_pin_set_dt(&mb_gpio0, level ? 1 : 0);
#else
    (void)pin;
    (void)level;
    return -1;
#endif
}

int mb_hal_gpio_read(uint8_t pin, uint8_t *out_level) {
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
    int value;
    if (out_level == NULL || pin != 0 || !device_is_ready(mb_gpio0.port)) {
        return -1;
    }
    if (gpio_pin_configure_dt(&mb_gpio0, GPIO_INPUT) != 0) {
        return -1;
    }
    value = gpio_pin_get_dt(&mb_gpio0);
    if (value < 0) {
        return -1;
    }
    *out_level = (uint8_t)value;
    return 0;
#else
    (void)pin;
    if (out_level != NULL) {
        *out_level = 0;
    }
    return -1;
#endif
}

int mb_hal_pwm_set_duty(uint8_t channel, uint16_t permille) {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm0), okay)
    uint32_t period_ns = 20000U;
    uint32_t pulse_ns;

    if (!device_is_ready(mb_pwm0) || permille > 1000) {
        return -1;
    }

    pulse_ns = (period_ns * permille) / 1000U;
    return pwm_set(mb_pwm0, channel, period_ns, pulse_ns, PWM_POLARITY_NORMAL);
#else
    (void)channel;
    (void)permille;
    return -1;
#endif
}

int mb_hal_pwm_config(uint8_t channel, uint32_t frequency_hz) {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm0), okay)
    uint32_t period_ns;
    if (frequency_hz == 0 || !device_is_ready(mb_pwm0)) {
        return -1;
    }
    period_ns = 1000000000U / frequency_hz;
    return pwm_set(mb_pwm0, channel, period_ns, 0U, PWM_POLARITY_NORMAL);
#else
    (void)channel;
    (void)frequency_hz;
    return -1;
#endif
}

int mb_hal_i2c_read_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *out_value) {
#if MB_HAS_I2C0 || MB_HAS_I2C1
    const struct device *i2c_dev = mb_get_i2c_bus(bus);
    if (out_value == NULL || i2c_dev == NULL) {
        return -1;
    }
    return i2c_write_read(i2c_dev, addr, &reg, 1U, out_value, 1U);
#else
    (void)bus;
    (void)addr;
    (void)reg;
    if (out_value != NULL) {
        *out_value = 0;
    }
    return -1;
#endif
}

int mb_hal_i2c_write_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t value) {
#if MB_HAS_I2C0 || MB_HAS_I2C1
    uint8_t tx[2] = {reg, value};
    const struct device *i2c_dev = mb_get_i2c_bus(bus);
    if (i2c_dev == NULL) {
        return -1;
    }
    return i2c_write(i2c_dev, tx, sizeof(tx), addr);
#else
    (void)bus;
    (void)addr;
    (void)reg;
    (void)value;
    return -1;
#endif
}

uint32_t mb_hal_monotonic_ms(void) {
    return k_uptime_get_32();
}

void mb_hal_delay_ms(uint32_t delay_ms) {
    k_msleep(delay_ms);
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
