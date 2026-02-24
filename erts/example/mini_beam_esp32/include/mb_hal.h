#ifndef MB_HAL_H
#define MB_HAL_H

/**
 * @file mb_hal.h
 * @brief Hardware Abstraction Layer used by VM BIF calls.
 *
 * The VM never touches board drivers directly. All side effects are routed
 * through these functions so host tests and embedded targets can share the
 * same VM core.
 */

#include <stdint.h>

/** Drive a digital output pin. */
int mb_hal_gpio_write(uint8_t pin, uint8_t level);
/** Read a digital input pin. */
int mb_hal_gpio_read(uint8_t pin, uint8_t *out_level);
/** Set PWM duty in permille [0..1000]. */
int mb_hal_pwm_set_duty(uint8_t channel, uint16_t permille);
/** Configure PWM base frequency for a channel. */
int mb_hal_pwm_config(uint8_t channel, uint32_t frequency_hz);
/** Read one I2C register from device @p addr on @p bus. */
int mb_hal_i2c_read_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *out_value);
/** Write one I2C register to device @p addr on @p bus. */
int mb_hal_i2c_write_reg(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t value);
/** Monotonic time source in milliseconds. */
uint32_t mb_hal_monotonic_ms(void);
/** Blocking delay for cooperative VM loops. */
void mb_hal_delay_ms(uint32_t delay_ms);

#endif
