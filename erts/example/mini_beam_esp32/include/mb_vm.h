#ifndef MB_VM_H
#define MB_VM_H

/**
 * @file mb_vm.h
 * @brief Mini BEAM-like register VM and mailbox API.
 *
 * This VM executes a compact bytecode stream over a fixed register file and
 * interacts with hardware exclusively through BIF calls in the HAL layer.
 * External control enters through a bounded mailbox carrying validated
 * commands (`mb_command_t`).
 */

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

/**
 * @brief Initialize VM state and attach a bytecode program.
 *
 * @param vm VM instance to initialize.
 * @param program Pointer to bytecode buffer (can be NULL for mailbox-only use).
 * @param program_size Size of the bytecode buffer in bytes.
 */
void mb_vm_init(mb_vm_t *vm, const uint8_t *program, size_t program_size);

/**
 * @brief Execute one VM instruction.
 *
 * @param vm VM instance.
 * @return MB_OK on success, otherwise a VM error code.
 */
int mb_vm_step(mb_vm_t *vm);

/**
 * @brief Execute up to @p max_steps VM instructions.
 *
 * Stops early on halt or error.
 *
 * @param vm VM instance.
 * @param max_steps Maximum instruction count for this run slice.
 * @return MB_OK on success, otherwise a VM error code.
 */
int mb_vm_run(mb_vm_t *vm, uint32_t max_steps);

/**
 * @brief Push a validated command into the VM mailbox.
 *
 * @param vm VM instance.
 * @param cmd Command to enqueue.
 * @return MB_OK, MB_MAILBOX_FULL, MB_INVALID_COMMAND, or MB_BAD_ARGUMENT.
 */
int mb_vm_mailbox_push(mb_vm_t *vm, mb_command_t cmd);

/**
 * @brief Pop one command from the VM mailbox.
 *
 * @param vm VM instance.
 * @param cmd Output command.
 * @return MB_OK or MB_MAILBOX_EMPTY.
 */
int mb_vm_mailbox_pop(mb_vm_t *vm, mb_command_t *cmd);

#endif
