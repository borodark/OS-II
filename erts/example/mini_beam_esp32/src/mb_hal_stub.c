#include "mb_hal.h"

#include <stdio.h>
#include <time.h>

int mb_hal_gpio_write(uint8_t pin, uint8_t level) {
    printf("[HAL] gpio_write pin=%u level=%u\n", pin, level);
    return 0;
}

int mb_hal_gpio_read(uint8_t pin, uint8_t *out_level) {
    if (out_level == NULL) {
        return -1;
    }
    *out_level = (uint8_t)(pin & 0x1);
    printf("[HAL] gpio_read pin=%u -> %u\n", pin, *out_level);
    return 0;
}

int mb_hal_pwm_set_duty(uint8_t channel, uint16_t permille) {
    if (permille > 1000) {
        return -1;
    }
    printf("[HAL] pwm_set_duty channel=%u permille=%u\n", channel, permille);
    return 0;
}

int mb_hal_pwm_config(uint8_t channel, uint32_t frequency_hz) {
    if (frequency_hz == 0 || frequency_hz > 40000) {
        return -1;
    }
    printf("[HAL] pwm_config channel=%u freq=%u\n", channel, frequency_hz);
    return 0;
}

int mb_hal_i2c_read_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *out_value) {
    uint8_t synthetic = (uint8_t)(addr ^ reg ^ bus);
    *out_value = synthetic;
    printf("[HAL] i2c_read bus=%u addr=0x%02x reg=0x%02x -> 0x%02x\n", bus, addr, reg, synthetic);
    return 0;
}

int mb_hal_i2c_write_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t value) {
    printf("[HAL] i2c_write bus=%u addr=0x%02x reg=0x%02x val=0x%02x\n", bus, addr, reg, value);
    return 0;
}

uint32_t mb_hal_monotonic_ms(void) {
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000ULL) + (ts.tv_nsec / 1000000ULL));
}

void mb_hal_delay_ms(uint32_t delay_ms) {
    struct timespec ts;
    ts.tv_sec = delay_ms / 1000U;
    ts.tv_nsec = (long)((delay_ms % 1000U) * 1000000UL);
    (void)nanosleep(&ts, NULL);
}
