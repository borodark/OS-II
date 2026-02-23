#ifndef MB_VM_H
#define MB_VM_H

#include <stddef.h>
#include <stdint.h>

#include "mb_errors.h"
#include "mb_types.h"

typedef enum {
    MB_BIF_GPIO_WRITE = 1,
    MB_BIF_PWM_SET_DUTY = 2,
    MB_BIF_I2C_READ_REG = 3,
    MB_BIF_MONOTONIC_MS = 4,
    MB_BIF_GPIO_READ = 5,
    MB_BIF_I2C_WRITE_REG = 6,
    MB_BIF_PWM_CONFIG = 7
} mb_bif_t;

typedef enum {
    MB_OP_NOP = 0x00,
    MB_OP_CONST_I32 = 0x01,
    MB_OP_MOVE = 0x02,
    MB_OP_ADD = 0x03,
    MB_OP_SUB = 0x04,
    MB_OP_CALL_BIF = 0x10,
    MB_OP_RECV_CMD = 0x20,
    MB_OP_JMP = 0x30,
    MB_OP_JMP_IF_ZERO = 0x31,
    MB_OP_SLEEP_MS = 0x40,
    MB_OP_HALT = 0xFF
} mb_opcode_t;

typedef struct {
    const uint8_t *program;
    size_t program_size;
    size_t pc;
    int32_t regs[MB_REG_COUNT];
    mb_mailbox_t mailbox;
    int halted;
    int last_error;
} mb_vm_t;

void mb_vm_init(mb_vm_t *vm, const uint8_t *program, size_t program_size);
int mb_vm_step(mb_vm_t *vm);
int mb_vm_run(mb_vm_t *vm, uint32_t max_steps);

int mb_vm_mailbox_push(mb_vm_t *vm, mb_command_t cmd);
int mb_vm_mailbox_pop(mb_vm_t *vm, mb_command_t *cmd);

#endif
