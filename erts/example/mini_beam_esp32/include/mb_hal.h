#ifndef MB_HAL_H
#define MB_HAL_H

#include <stdint.h>

int mb_hal_gpio_write(uint8_t pin, uint8_t level);
int mb_hal_gpio_read(uint8_t pin, uint8_t *out_level);
int mb_hal_pwm_set_duty(uint8_t channel, uint16_t permille);
int mb_hal_pwm_config(uint8_t channel, uint32_t frequency_hz);
int mb_hal_i2c_read_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *out_value);
int mb_hal_i2c_write_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t value);
uint32_t mb_hal_monotonic_ms(void);
void mb_hal_delay_ms(uint32_t delay_ms);

#endif
