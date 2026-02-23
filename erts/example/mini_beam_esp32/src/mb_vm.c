#include "mb_vm.h"

#include <string.h>

#include "mb_hal.h"

#define MB_MAX_GPIO_PIN 39
#define MB_MAX_PWM_CHANNEL 7
#define MB_MAX_I2C_BUS 3
#define MB_MAX_I2C_ADDR 0x7f
#define MB_MAX_PWM_PERMILLE 1000
#define MB_MAX_PWM_FREQUENCY_HZ 40000

static int mb_vm_fetch_u8(mb_vm_t *vm, uint8_t *out) {
    if (vm->pc >= vm->program_size) {
        return MB_EOF;
    }
    *out = vm->program[vm->pc++];
    return MB_OK;
}

static int mb_vm_fetch_i32(mb_vm_t *vm, int32_t *out) {
    uint8_t b0, b1, b2, b3;
    if (mb_vm_fetch_u8(vm, &b0) != MB_OK ||
        mb_vm_fetch_u8(vm, &b1) != MB_OK ||
        mb_vm_fetch_u8(vm, &b2) != MB_OK ||
        mb_vm_fetch_u8(vm, &b3) != MB_OK) {
        return MB_EOF;
    }
    *out = (int32_t)((uint32_t)b0 |
                     ((uint32_t)b1 << 8) |
                     ((uint32_t)b2 << 16) |
                     ((uint32_t)b3 << 24));
    return MB_OK;
}

static int mb_vm_valid_reg(uint8_t reg) {
    return reg < MB_REG_COUNT;
}

static int mb_validate_gpio_pin(int32_t pin) {
    return pin >= 0 && pin <= MB_MAX_GPIO_PIN;
}

static int mb_validate_pwm_channel(int32_t channel) {
    return channel >= 0 && channel <= MB_MAX_PWM_CHANNEL;
}

static int mb_validate_pwm_duty(int32_t permille) {
    return permille >= 0 && permille <= MB_MAX_PWM_PERMILLE;
}

static int mb_validate_i2c_bus(int32_t bus) {
    return bus >= 0 && bus <= MB_MAX_I2C_BUS;
}

static int mb_validate_i2c_addr(int32_t addr) {
    return addr >= 0 && addr <= MB_MAX_I2C_ADDR;
}

static int mb_validate_u8_value(int32_t value) {
    return value >= 0 && value <= 0xff;
}

static int mb_validate_pwm_frequency(int32_t frequency_hz) {
    return frequency_hz > 0 && frequency_hz <= MB_MAX_PWM_FREQUENCY_HZ;
}

static int mb_validate_command(const mb_command_t *cmd) {
    switch ((mb_command_type_t)cmd->type) {
    case MB_CMD_NONE:
        return MB_OK;
    case MB_CMD_GPIO_WRITE:
        return (mb_validate_gpio_pin(cmd->a) && (cmd->b == 0 || cmd->b == 1)) ? MB_OK : MB_BAD_ARGUMENT;
    case MB_CMD_GPIO_READ:
        return mb_validate_gpio_pin(cmd->a) ? MB_OK : MB_BAD_ARGUMENT;
    case MB_CMD_PWM_SET_DUTY:
        return (mb_validate_pwm_channel(cmd->a) && mb_validate_pwm_duty(cmd->b)) ? MB_OK : MB_BAD_ARGUMENT;
    case MB_CMD_PWM_CONFIG:
        return (mb_validate_pwm_channel(cmd->a) && mb_validate_pwm_frequency(cmd->b)) ? MB_OK : MB_BAD_ARGUMENT;
    case MB_CMD_I2C_READ:
        return (mb_validate_i2c_bus(cmd->a) && mb_validate_i2c_addr(cmd->b) && mb_validate_u8_value(cmd->c)) ? MB_OK : MB_BAD_ARGUMENT;
    case MB_CMD_I2C_WRITE:
        return (mb_validate_i2c_bus(cmd->a) && mb_validate_i2c_addr(cmd->b) &&
                mb_validate_u8_value(cmd->c) && mb_validate_u8_value(cmd->d))
                   ? MB_OK
                   : MB_BAD_ARGUMENT;
    default:
        return MB_INVALID_COMMAND;
    }
}

void mb_vm_init(mb_vm_t *vm, const uint8_t *program, size_t program_size) {
    memset(vm, 0, sizeof(*vm));
    vm->program = program;
    vm->program_size = program_size;
}

int mb_vm_mailbox_push(mb_vm_t *vm, mb_command_t cmd) {
    mb_mailbox_t *mb = &vm->mailbox;
    int status = mb_validate_command(&cmd);

    if (status != MB_OK) {
        return status;
    }

    if (mb->count >= MB_MAILBOX_CAPACITY) {
        return MB_MAILBOX_FULL;
    }

    mb->items[mb->tail] = cmd;
    mb->tail = (mb->tail + 1U) % MB_MAILBOX_CAPACITY;
    mb->count++;
    return MB_OK;
}

int mb_vm_mailbox_pop(mb_vm_t *vm, mb_command_t *cmd) {
    mb_mailbox_t *mb = &vm->mailbox;
    if (mb->count == 0) {
        return MB_MAILBOX_EMPTY;
    }
    *cmd = mb->items[mb->head];
    mb->head = (mb->head + 1U) % MB_MAILBOX_CAPACITY;
    mb->count--;
    return MB_OK;
}

static int mb_vm_call_bif(mb_vm_t *vm, uint8_t bif_id, uint8_t argc, uint8_t *argv, uint8_t dst) {
    int rc = 0;
    if (!mb_vm_valid_reg(dst)) {
        return MB_BAD_REG;
    }

    switch ((mb_bif_t)bif_id) {
    case MB_BIF_GPIO_WRITE:
        if (argc != 2) {
            return MB_BAD_ARGC;
        }
        if (!mb_validate_gpio_pin(vm->regs[argv[0]]) || (vm->regs[argv[1]] != 0 && vm->regs[argv[1]] != 1)) {
            return MB_BAD_ARGUMENT;
        }
        rc = mb_hal_gpio_write((uint8_t)vm->regs[argv[0]], (uint8_t)vm->regs[argv[1]]);
        vm->regs[dst] = rc;
        return MB_OK;

    case MB_BIF_GPIO_READ: {
        uint8_t level = 0;
        if (argc != 1) {
            return MB_BAD_ARGC;
        }
        if (!mb_validate_gpio_pin(vm->regs[argv[0]])) {
            return MB_BAD_ARGUMENT;
        }
        rc = mb_hal_gpio_read((uint8_t)vm->regs[argv[0]], &level);
        vm->regs[dst] = (rc == 0) ? (int32_t)level : rc;
        return MB_OK;
    }

    case MB_BIF_PWM_SET_DUTY:
        if (argc != 2) {
            return MB_BAD_ARGC;
        }
        if (!mb_validate_pwm_channel(vm->regs[argv[0]]) || !mb_validate_pwm_duty(vm->regs[argv[1]])) {
            return MB_BAD_ARGUMENT;
        }
        rc = mb_hal_pwm_set_duty((uint8_t)vm->regs[argv[0]], (uint16_t)vm->regs[argv[1]]);
        vm->regs[dst] = rc;
        return MB_OK;

    case MB_BIF_PWM_CONFIG:
        if (argc != 2) {
            return MB_BAD_ARGC;
        }
        if (!mb_validate_pwm_channel(vm->regs[argv[0]]) || !mb_validate_pwm_frequency(vm->regs[argv[1]])) {
            return MB_BAD_ARGUMENT;
        }
        rc = mb_hal_pwm_config((uint8_t)vm->regs[argv[0]], (uint32_t)vm->regs[argv[1]]);
        vm->regs[dst] = rc;
        return MB_OK;

    case MB_BIF_I2C_READ_REG: {
        uint8_t value = 0;
        if (argc != 3) {
            return MB_BAD_ARGC;
        }
        if (!mb_validate_i2c_bus(vm->regs[argv[0]]) || !mb_validate_i2c_addr(vm->regs[argv[1]]) ||
            !mb_validate_u8_value(vm->regs[argv[2]])) {
            return MB_BAD_ARGUMENT;
        }
        rc = mb_hal_i2c_read_reg((uint8_t)vm->regs[argv[0]],
                                 (uint8_t)vm->regs[argv[1]],
                                 (uint8_t)vm->regs[argv[2]],
                                 &value);
        vm->regs[dst] = (rc == 0) ? (int32_t)value : rc;
        return MB_OK;
    }

    case MB_BIF_I2C_WRITE_REG:
        if (argc != 4) {
            return MB_BAD_ARGC;
        }
        if (!mb_validate_i2c_bus(vm->regs[argv[0]]) || !mb_validate_i2c_addr(vm->regs[argv[1]]) ||
            !mb_validate_u8_value(vm->regs[argv[2]]) || !mb_validate_u8_value(vm->regs[argv[3]])) {
            return MB_BAD_ARGUMENT;
        }
        rc = mb_hal_i2c_write_reg((uint8_t)vm->regs[argv[0]],
                                  (uint8_t)vm->regs[argv[1]],
                                  (uint8_t)vm->regs[argv[2]],
                                  (uint8_t)vm->regs[argv[3]]);
        vm->regs[dst] = rc;
        return MB_OK;

    case MB_BIF_MONOTONIC_MS:
        if (argc != 0) {
            return MB_BAD_ARGC;
        }
        vm->regs[dst] = (int32_t)mb_hal_monotonic_ms();
        return MB_OK;

    default:
        return MB_BAD_BIF;
    }
}

int mb_vm_step(mb_vm_t *vm) {
    uint8_t op;
    int32_t val;

    if (vm->halted) {
        return MB_OK;
    }

    if (mb_vm_fetch_u8(vm, &op) != MB_OK) {
        vm->last_error = MB_EOF;
        return vm->last_error;
    }

    switch ((mb_opcode_t)op) {
    case MB_OP_NOP:
        return MB_OK;

    case MB_OP_CONST_I32: {
        uint8_t dst;
        if (mb_vm_fetch_u8(vm, &dst) != MB_OK || mb_vm_fetch_i32(vm, &val) != MB_OK) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        if (!mb_vm_valid_reg(dst)) {
            vm->last_error = MB_BAD_REG;
            return vm->last_error;
        }
        vm->regs[dst] = val;
        return MB_OK;
    }

    case MB_OP_MOVE:
    case MB_OP_ADD:
    case MB_OP_SUB: {
        uint8_t dst, a, b;
        if (mb_vm_fetch_u8(vm, &dst) != MB_OK ||
            mb_vm_fetch_u8(vm, &a) != MB_OK ||
            mb_vm_fetch_u8(vm, &b) != MB_OK) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        if (!mb_vm_valid_reg(dst) || !mb_vm_valid_reg(a) || !mb_vm_valid_reg(b)) {
            vm->last_error = MB_BAD_REG;
            return vm->last_error;
        }
        if (op == MB_OP_MOVE) {
            vm->regs[dst] = vm->regs[a];
        } else if (op == MB_OP_ADD) {
            vm->regs[dst] = vm->regs[a] + vm->regs[b];
        } else {
            vm->regs[dst] = vm->regs[a] - vm->regs[b];
        }
        return MB_OK;
    }

    case MB_OP_CALL_BIF: {
        uint8_t bif, argc, i, dst;
        uint8_t args[8];
        if (mb_vm_fetch_u8(vm, &bif) != MB_OK || mb_vm_fetch_u8(vm, &argc) != MB_OK) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        if (argc > 8) {
            vm->last_error = MB_BAD_ARGC;
            return vm->last_error;
        }
        for (i = 0; i < argc; i++) {
            if (mb_vm_fetch_u8(vm, &args[i]) != MB_OK) {
                vm->last_error = MB_EOF;
                return vm->last_error;
            }
            if (!mb_vm_valid_reg(args[i])) {
                vm->last_error = MB_BAD_REG;
                return vm->last_error;
            }
        }
        if (mb_vm_fetch_u8(vm, &dst) != MB_OK) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        vm->last_error = mb_vm_call_bif(vm, bif, argc, args, dst);
        return vm->last_error;
    }

    case MB_OP_RECV_CMD: {
        uint8_t r_type, r_a, r_b, r_c, r_d;
        mb_command_t cmd;
        int rc;

        if (mb_vm_fetch_u8(vm, &r_type) != MB_OK ||
            mb_vm_fetch_u8(vm, &r_a) != MB_OK ||
            mb_vm_fetch_u8(vm, &r_b) != MB_OK ||
            mb_vm_fetch_u8(vm, &r_c) != MB_OK ||
            mb_vm_fetch_u8(vm, &r_d) != MB_OK) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        if (!mb_vm_valid_reg(r_type) || !mb_vm_valid_reg(r_a) ||
            !mb_vm_valid_reg(r_b) || !mb_vm_valid_reg(r_c) || !mb_vm_valid_reg(r_d)) {
            vm->last_error = MB_BAD_REG;
            return vm->last_error;
        }

        rc = mb_vm_mailbox_pop(vm, &cmd);
        if (rc == MB_OK) {
            rc = mb_validate_command(&cmd);
            if (rc == MB_OK) {
                vm->regs[r_type] = cmd.type;
                vm->regs[r_a] = cmd.a;
                vm->regs[r_b] = cmd.b;
                vm->regs[r_c] = cmd.c;
                vm->regs[r_d] = cmd.d;
                vm->last_error = MB_OK;
                return MB_OK;
            }
            vm->regs[r_type] = MB_CMD_NONE;
            vm->regs[r_a] = rc;
            vm->regs[r_b] = 0;
            vm->regs[r_c] = 0;
            vm->regs[r_d] = 0;
            vm->last_error = rc;
            return MB_OK;
        }

        vm->regs[r_type] = MB_CMD_NONE;
        vm->regs[r_a] = MB_MAILBOX_EMPTY;
        vm->regs[r_b] = 0;
        vm->regs[r_c] = 0;
        vm->regs[r_d] = 0;
        vm->last_error = MB_MAILBOX_EMPTY;
        return MB_OK;
    }

    case MB_OP_JMP: {
        int32_t offset;
        if (mb_vm_fetch_i32(vm, &offset) != MB_OK) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        if (offset < 0 && (size_t)(-offset) > vm->pc) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        vm->pc = (size_t)((int64_t)vm->pc + offset);
        if (vm->pc > vm->program_size) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        return MB_OK;
    }

    case MB_OP_JMP_IF_ZERO: {
        uint8_t reg;
        int32_t offset;
        if (mb_vm_fetch_u8(vm, &reg) != MB_OK || mb_vm_fetch_i32(vm, &offset) != MB_OK) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        if (!mb_vm_valid_reg(reg)) {
            vm->last_error = MB_BAD_REG;
            return vm->last_error;
        }
        if (vm->regs[reg] == 0) {
            if (offset < 0 && (size_t)(-offset) > vm->pc) {
                vm->last_error = MB_EOF;
                return vm->last_error;
            }
            vm->pc = (size_t)((int64_t)vm->pc + offset);
            if (vm->pc > vm->program_size) {
                vm->last_error = MB_EOF;
                return vm->last_error;
            }
        }
        return MB_OK;
    }

    case MB_OP_SLEEP_MS: {
        uint8_t reg;
        if (mb_vm_fetch_u8(vm, &reg) != MB_OK) {
            vm->last_error = MB_EOF;
            return vm->last_error;
        }
        if (!mb_vm_valid_reg(reg)) {
            vm->last_error = MB_BAD_REG;
            return vm->last_error;
        }
        mb_hal_delay_ms((uint32_t)vm->regs[reg]);
        return MB_OK;
    }

    case MB_OP_HALT:
        vm->halted = 1;
        return MB_OK;

    default:
        vm->last_error = MB_BAD_OPCODE;
        return vm->last_error;
    }
}

int mb_vm_run(mb_vm_t *vm, uint32_t max_steps) {
    uint32_t steps = 0;
    while (!vm->halted && steps < max_steps) {
        int rc = mb_vm_step(vm);
        if (rc != MB_OK) {
            return rc;
        }
        steps++;
    }
    return MB_OK;
}
