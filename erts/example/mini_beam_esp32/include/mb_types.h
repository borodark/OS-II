#ifndef MB_TYPES_H
#define MB_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define MB_REG_COUNT 16
#define MB_MAILBOX_CAPACITY 32

typedef enum {
    MB_CMD_NONE = 0,
    MB_CMD_GPIO_WRITE = 1,
    MB_CMD_PWM_SET_DUTY = 2,
    MB_CMD_I2C_READ = 3,
    MB_CMD_GPIO_READ = 4,
    MB_CMD_I2C_WRITE = 5,
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
