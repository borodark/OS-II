#ifndef MB_TYPES_H
#define MB_TYPES_H

/**
 * @file mb_types.h
 * @brief Shared VM ABI types for commands and mailbox state.
 *
 * These types form the command contract between orchestration code and the
 * mini VM runtime. Keep changes backward-compatible with event/contract docs.
 */

#include <stddef.h>
#include <stdint.h>

#define MB_REG_COUNT 16
#define MB_MAILBOX_CAPACITY 32

typedef enum {
    MB_CMD_NONE = 0,
    /** a=pin, b=level */
    MB_CMD_GPIO_WRITE = 1,
    /** a=channel, b=permille */
    MB_CMD_PWM_SET_DUTY = 2,
    /** a=bus, b=addr, c=reg, d=user sensor/actuator id */
    MB_CMD_I2C_READ = 3,
    /** a=pin */
    MB_CMD_GPIO_READ = 4,
    /** a=bus, b=addr, c=reg, d=value */
    MB_CMD_I2C_WRITE = 5,
    /** a=channel, b=frequency_hz */
    MB_CMD_PWM_CONFIG = 6
} mb_command_type_t;

typedef struct {
    int32_t type;
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
} mb_command_t;

typedef struct {
    mb_command_t items[MB_MAILBOX_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
} mb_mailbox_t;

#endif
