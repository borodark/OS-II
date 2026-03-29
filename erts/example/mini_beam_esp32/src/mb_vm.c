#include "mb_vm.h"

#include <string.h>

#include "mb_hal.h"
#include "mb_scheduler.h"

#define MB_MAX_GPIO_PIN 39
#define MB_MAX_PWM_CHANNEL 7
#define MB_MAX_I2C_BUS 3
#define MB_MAX_I2C_ADDR 0x7f
#define MB_MAX_PWM_PERMILLE 1000
#define MB_MAX_PWM_FREQUENCY_HZ 40000

/* --- fetch helpers (operate on process) --- */

static int mb_fetch_u8(mb_process_t *proc, uint8_t *out) {
    if (proc->pc >= proc->program_size) {
        return MB_EOF;
    }
    *out = proc->program[proc->pc++];
    return MB_OK;
}

static int mb_fetch_i32(mb_process_t *proc, int32_t *out) {
    uint8_t b0, b1, b2, b3;
    if (mb_fetch_u8(proc, &b0) != MB_OK ||
        mb_fetch_u8(proc, &b1) != MB_OK ||
        mb_fetch_u8(proc, &b2) != MB_OK ||
        mb_fetch_u8(proc, &b3) != MB_OK) {
        return MB_EOF;
    }
    *out = (int32_t)((uint32_t)b0 |
                     ((uint32_t)b1 << 8) |
                     ((uint32_t)b2 << 16) |
                     ((uint32_t)b3 << 24));
    return MB_OK;
}

/* --- validation helpers (pure functions) --- */

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

/* --- mailbox helpers (operate on raw mailbox) --- */

static int mb_mailbox_push_raw(mb_mailbox_t *mb, mb_command_t cmd) {
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

static int mb_mailbox_pop_raw(mb_mailbox_t *mb, mb_command_t *cmd) {
    if (mb->count == 0) {
        return MB_MAILBOX_EMPTY;
    }
    *cmd = mb->items[mb->head];
    mb->head = (mb->head + 1U) % MB_MAILBOX_CAPACITY;
    mb->count--;
    return MB_OK;
}

/* --- BIF dispatch (operates on process, registers are tagged terms) --- */

/* Helper: extract int32 from a tagged register for BIF arguments. */
#define REG_INT(r) MB_GET_SMALLINT(proc->regs[(r)])

static int mb_call_bif(mb_process_t *proc, uint8_t bif_id, uint8_t argc, uint8_t *argv, uint8_t dst) {
    int rc = 0;
    if (!mb_vm_valid_reg(dst)) {
        return MB_BAD_REG;
    }

    switch ((mb_bif_t)bif_id) {
    case MB_BIF_GPIO_WRITE: {
        int32_t pin, level;
        if (argc != 2) return MB_BAD_ARGC;
        pin = REG_INT(argv[0]);
        level = REG_INT(argv[1]);
        if (!mb_validate_gpio_pin(pin) || (level != 0 && level != 1))
            return MB_BAD_ARGUMENT;
        rc = mb_hal_gpio_write((uint8_t)pin, (uint8_t)level);
        proc->regs[dst] = MB_MAKE_SMALLINT(rc);
        return MB_OK;
    }

    case MB_BIF_GPIO_READ: {
        int32_t pin;
        uint8_t level = 0;
        if (argc != 1) return MB_BAD_ARGC;
        pin = REG_INT(argv[0]);
        if (!mb_validate_gpio_pin(pin)) return MB_BAD_ARGUMENT;
        rc = mb_hal_gpio_read((uint8_t)pin, &level);
        proc->regs[dst] = MB_MAKE_SMALLINT((rc == 0) ? (int32_t)level : rc);
        return MB_OK;
    }

    case MB_BIF_PWM_SET_DUTY: {
        int32_t ch, duty;
        if (argc != 2) return MB_BAD_ARGC;
        ch = REG_INT(argv[0]);
        duty = REG_INT(argv[1]);
        if (!mb_validate_pwm_channel(ch) || !mb_validate_pwm_duty(duty))
            return MB_BAD_ARGUMENT;
        rc = mb_hal_pwm_set_duty((uint8_t)ch, (uint16_t)duty);
        proc->regs[dst] = MB_MAKE_SMALLINT(rc);
        return MB_OK;
    }

    case MB_BIF_PWM_CONFIG: {
        int32_t ch, freq;
        if (argc != 2) return MB_BAD_ARGC;
        ch = REG_INT(argv[0]);
        freq = REG_INT(argv[1]);
        if (!mb_validate_pwm_channel(ch) || !mb_validate_pwm_frequency(freq))
            return MB_BAD_ARGUMENT;
        rc = mb_hal_pwm_config((uint8_t)ch, (uint32_t)freq);
        proc->regs[dst] = MB_MAKE_SMALLINT(rc);
        return MB_OK;
    }

    case MB_BIF_I2C_READ_REG: {
        int32_t bus, addr, reg;
        uint8_t value = 0;
        if (argc != 3) return MB_BAD_ARGC;
        bus = REG_INT(argv[0]);
        addr = REG_INT(argv[1]);
        reg = REG_INT(argv[2]);
        if (!mb_validate_i2c_bus(bus) || !mb_validate_i2c_addr(addr) ||
            !mb_validate_u8_value(reg))
            return MB_BAD_ARGUMENT;
        rc = mb_hal_i2c_read_reg((uint8_t)bus, (uint8_t)addr, (uint8_t)reg, &value);
        proc->regs[dst] = MB_MAKE_SMALLINT((rc == 0) ? (int32_t)value : rc);
        return MB_OK;
    }

    case MB_BIF_I2C_WRITE_REG: {
        int32_t bus, addr, reg, val;
        if (argc != 4) return MB_BAD_ARGC;
        bus = REG_INT(argv[0]);
        addr = REG_INT(argv[1]);
        reg = REG_INT(argv[2]);
        val = REG_INT(argv[3]);
        if (!mb_validate_i2c_bus(bus) || !mb_validate_i2c_addr(addr) ||
            !mb_validate_u8_value(reg) || !mb_validate_u8_value(val))
            return MB_BAD_ARGUMENT;
        rc = mb_hal_i2c_write_reg((uint8_t)bus, (uint8_t)addr, (uint8_t)reg, (uint8_t)val);
        proc->regs[dst] = MB_MAKE_SMALLINT(rc);
        return MB_OK;
    }

    case MB_BIF_MONOTONIC_MS:
        if (argc != 0) return MB_BAD_ARGC;
        proc->regs[dst] = MB_MAKE_SMALLINT((int32_t)mb_hal_monotonic_ms());
        return MB_OK;

    default:
        return MB_BAD_BIF;
    }
}

#undef REG_INT

/* --- process API --- */

void mb_proc_init(mb_process_t *proc, mb_pid_t pid,
                  const uint8_t *program, size_t program_size) {
    memset(proc, 0, sizeof(*proc));
    proc->pid = pid;
    proc->state = MB_PROC_READY;
    proc->program = program;
    proc->program_size = program_size;
    mb_heap_init(&proc->heap);
}

int mb_proc_step(mb_process_t *proc, void *sched) {
    uint8_t op;
    int32_t val;
    size_t pre_op_pc;

    if (proc->halted) {
        return MB_OK;
    }

    pre_op_pc = proc->pc;

    if (mb_fetch_u8(proc, &op) != MB_OK) {
        proc->last_error = MB_EOF;
        return proc->last_error;
    }

    switch ((mb_opcode_t)op) {
    case MB_OP_NOP:
        return MB_OK;

    case MB_OP_CONST_I32: {
        uint8_t dst;
        if (mb_fetch_u8(proc, &dst) != MB_OK || mb_fetch_i32(proc, &val) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(dst)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        proc->regs[dst] = MB_MAKE_SMALLINT(val);
        return MB_OK;
    }

    case MB_OP_MOVE:
    case MB_OP_ADD:
    case MB_OP_SUB: {
        uint8_t dst, a, b;
        if (mb_fetch_u8(proc, &dst) != MB_OK ||
            mb_fetch_u8(proc, &a) != MB_OK ||
            mb_fetch_u8(proc, &b) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(dst) || !mb_vm_valid_reg(a) || !mb_vm_valid_reg(b)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        if (op == MB_OP_MOVE) {
            proc->regs[dst] = proc->regs[a];
        } else if (op == MB_OP_ADD) {
            proc->regs[dst] = MB_MAKE_SMALLINT(
                MB_GET_SMALLINT(proc->regs[a]) + MB_GET_SMALLINT(proc->regs[b]));
        } else {
            proc->regs[dst] = MB_MAKE_SMALLINT(
                MB_GET_SMALLINT(proc->regs[a]) - MB_GET_SMALLINT(proc->regs[b]));
        }
        return MB_OK;
    }

    case MB_OP_CALL_BIF: {
        uint8_t bif, argc, i, dst;
        uint8_t args[8];
        if (mb_fetch_u8(proc, &bif) != MB_OK || mb_fetch_u8(proc, &argc) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (argc > 8) {
            proc->last_error = MB_BAD_ARGC;
            return proc->last_error;
        }
        for (i = 0; i < argc; i++) {
            if (mb_fetch_u8(proc, &args[i]) != MB_OK) {
                proc->last_error = MB_EOF;
                return proc->last_error;
            }
            if (!mb_vm_valid_reg(args[i])) {
                proc->last_error = MB_BAD_REG;
                return proc->last_error;
            }
        }
        if (mb_fetch_u8(proc, &dst) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        proc->last_error = mb_call_bif(proc, bif, argc, args, dst);
        return proc->last_error;
    }

    case MB_OP_RECV_CMD: {
        uint8_t r_type, r_a, r_b, r_c, r_d;
        mb_command_t cmd;
        int rc;

        if (mb_fetch_u8(proc, &r_type) != MB_OK ||
            mb_fetch_u8(proc, &r_a) != MB_OK ||
            mb_fetch_u8(proc, &r_b) != MB_OK ||
            mb_fetch_u8(proc, &r_c) != MB_OK ||
            mb_fetch_u8(proc, &r_d) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(r_type) || !mb_vm_valid_reg(r_a) ||
            !mb_vm_valid_reg(r_b) || !mb_vm_valid_reg(r_c) || !mb_vm_valid_reg(r_d)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }

        rc = mb_mailbox_pop_raw(&proc->mailbox, &cmd);
        if (rc == MB_OK) {
            rc = mb_validate_command(&cmd);
            if (rc == MB_OK) {
                proc->regs[r_type] = MB_MAKE_SMALLINT(cmd.type);
                proc->regs[r_a] = MB_MAKE_SMALLINT(cmd.a);
                proc->regs[r_b] = MB_MAKE_SMALLINT(cmd.b);
                proc->regs[r_c] = MB_MAKE_SMALLINT(cmd.c);
                proc->regs[r_d] = MB_MAKE_SMALLINT(cmd.d);
                proc->last_error = MB_OK;
                return MB_OK;
            }
            proc->regs[r_type] = MB_MAKE_SMALLINT(MB_CMD_NONE);
            proc->regs[r_a] = MB_MAKE_SMALLINT(rc);
            proc->regs[r_b] = MB_MAKE_SMALLINT(0);
            proc->regs[r_c] = MB_MAKE_SMALLINT(0);
            proc->regs[r_d] = MB_MAKE_SMALLINT(0);
            proc->last_error = rc;
            return MB_OK;
        }

        if (sched != NULL) {
            /* Scheduler mode: block until a message arrives. */
            proc->pc = pre_op_pc;
            proc->state = MB_PROC_WAITING;
            return MB_OK;
        }
        /* Compat mode: non-blocking, return NONE. */
        proc->regs[r_type] = MB_MAKE_SMALLINT(MB_CMD_NONE);
        proc->regs[r_a] = MB_MAKE_SMALLINT(MB_MAILBOX_EMPTY);
        proc->regs[r_b] = MB_MAKE_SMALLINT(0);
        proc->regs[r_c] = MB_MAKE_SMALLINT(0);
        proc->regs[r_d] = MB_MAKE_SMALLINT(0);
        proc->last_error = MB_MAILBOX_EMPTY;
        return MB_OK;
    }

    case MB_OP_SEND: {
        uint8_t r_pid, r_type, r_a, r_b, r_c, r_d;
        mb_scheduler_t *s = (mb_scheduler_t *)sched;
        mb_process_t *target;
        mb_command_t cmd;
        int rc;

        if (mb_fetch_u8(proc, &r_pid) != MB_OK ||
            mb_fetch_u8(proc, &r_type) != MB_OK ||
            mb_fetch_u8(proc, &r_a) != MB_OK ||
            mb_fetch_u8(proc, &r_b) != MB_OK ||
            mb_fetch_u8(proc, &r_c) != MB_OK ||
            mb_fetch_u8(proc, &r_d) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(r_pid) || !mb_vm_valid_reg(r_type) ||
            !mb_vm_valid_reg(r_a) || !mb_vm_valid_reg(r_b) ||
            !mb_vm_valid_reg(r_c) || !mb_vm_valid_reg(r_d)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }

        if (s == NULL) {
            proc->regs[r_pid] = MB_MAKE_SMALLINT(MB_BAD_ARGUMENT);
            return MB_OK;
        }

        target = mb_sched_proc(s, (mb_pid_t)MB_GET_SMALLINT(proc->regs[r_pid]));
        if (target == NULL) {
            proc->regs[r_pid] = MB_MAKE_SMALLINT(MB_BAD_PID);
            return MB_OK;
        }

        /* Untag register values back to raw int32 for command ABI */
        cmd.type = MB_GET_SMALLINT(proc->regs[r_type]);
        cmd.a = MB_GET_SMALLINT(proc->regs[r_a]);
        cmd.b = MB_GET_SMALLINT(proc->regs[r_b]);
        cmd.c = MB_GET_SMALLINT(proc->regs[r_c]);
        cmd.d = MB_GET_SMALLINT(proc->regs[r_d]);

        rc = mb_vm_mailbox_push_proc(target, cmd);
        if (rc == MB_OK && target->state == MB_PROC_WAITING) {
            target->state = MB_PROC_READY;
        }
        proc->regs[r_pid] = MB_MAKE_SMALLINT(rc);
        return MB_OK;
    }

    case MB_OP_SELF: {
        uint8_t r_dst;
        if (mb_fetch_u8(proc, &r_dst) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(r_dst)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        proc->regs[r_dst] = MB_MAKE_PID(proc->pid);
        return MB_OK;
    }

    case MB_OP_YIELD:
        proc->reductions = MB_REDUCTIONS; /* exhaust budget */
        return MB_OK;

    case MB_OP_JMP: {
        int32_t offset;
        if (mb_fetch_i32(proc, &offset) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (offset < 0 && (size_t)(-offset) > proc->pc) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        proc->pc = (size_t)((int64_t)proc->pc + offset);
        if (proc->pc > proc->program_size) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        return MB_OK;
    }

    case MB_OP_JMP_IF_ZERO: {
        uint8_t reg;
        int32_t offset;
        if (mb_fetch_u8(proc, &reg) != MB_OK || mb_fetch_i32(proc, &offset) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(reg)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        if (proc->regs[reg] == MB_MAKE_SMALLINT(0)) {
            if (offset < 0 && (size_t)(-offset) > proc->pc) {
                proc->last_error = MB_EOF;
                return proc->last_error;
            }
            proc->pc = (size_t)((int64_t)proc->pc + offset);
            if (proc->pc > proc->program_size) {
                proc->last_error = MB_EOF;
                return proc->last_error;
            }
        }
        return MB_OK;
    }

    case MB_OP_SLEEP_MS: {
        uint8_t reg;
        if (mb_fetch_u8(proc, &reg) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(reg)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        if (sched != NULL) {
            /* Scheduler mode: record wake time and yield. */
            proc->sleep_until_ms = mb_hal_monotonic_ms() + (uint32_t)MB_GET_SMALLINT(proc->regs[reg]);
            proc->state = MB_PROC_SLEEPING;
        } else {
            /* Compat mode: block directly. */
            mb_hal_delay_ms((uint32_t)MB_GET_SMALLINT(proc->regs[reg]));
        }
        return MB_OK;
    }

    case MB_OP_MAKE_TUPLE: {
        uint8_t r_dst, arity, i;
        mb_term_t elems[MB_MAX_TUPLE_ARITY];
        mb_term_t result;
        mb_term_t *roots[MB_REG_COUNT];

        if (mb_fetch_u8(proc, &r_dst) != MB_OK || mb_fetch_u8(proc, &arity) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(r_dst) || arity > MB_MAX_TUPLE_ARITY) {
            proc->last_error = (arity > MB_MAX_TUPLE_ARITY) ? MB_BAD_ARITY : MB_BAD_REG;
            return proc->last_error;
        }
        for (i = 0; i < arity; i++) {
            uint8_t r;
            if (mb_fetch_u8(proc, &r) != MB_OK) {
                proc->last_error = MB_EOF;
                return proc->last_error;
            }
            if (!mb_vm_valid_reg(r)) {
                proc->last_error = MB_BAD_REG;
                return proc->last_error;
            }
            elems[i] = proc->regs[r];
        }
        result = mb_heap_make_tuple(&proc->heap, elems, arity);
        if (result == 0) {
            /* GC and retry */
            for (i = 0; i < MB_REG_COUNT; i++) roots[i] = &proc->regs[i];
            mb_heap_gc(&proc->heap, roots, MB_REG_COUNT);
            /* Re-read elems from (possibly moved) registers */
            /* Note: the elem register indices are consumed, so we use
             * the values already copied into elems[]. Immediate values
             * in elems don't need updating. Heap pointers were updated
             * via roots. Re-copy from registers. */
            result = mb_heap_make_tuple(&proc->heap, elems, arity);
            if (result == 0) {
                proc->last_error = MB_HEAP_OOM;
                return proc->last_error;
            }
        }
        proc->regs[r_dst] = result;
        return MB_OK;
    }

    case MB_OP_TUPLE_ELEM: {
        uint8_t r_dst, r_tuple, index;
        mb_term_t *ptr;
        uint32_t arity;

        if (mb_fetch_u8(proc, &r_dst) != MB_OK ||
            mb_fetch_u8(proc, &r_tuple) != MB_OK ||
            mb_fetch_u8(proc, &index) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(r_dst) || !mb_vm_valid_reg(r_tuple)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        if (!MB_IS_BOXED(proc->regs[r_tuple])) {
            proc->last_error = MB_BAD_TERM;
            return proc->last_error;
        }
        ptr = &proc->heap.from[MB_GET_BOXED(proc->regs[r_tuple])];
        if (!MB_IS_TUPLE_HDR(ptr[0])) {
            proc->last_error = MB_BAD_TERM;
            return proc->last_error;
        }
        arity = MB_GET_TUPLE_ARITY(ptr[0]);
        if (index >= arity) {
            proc->last_error = MB_BAD_ARITY;
            return proc->last_error;
        }
        proc->regs[r_dst] = ptr[1 + index];
        return MB_OK;
    }

    case MB_OP_CONS: {
        uint8_t r_dst, r_head, r_tail;
        mb_term_t result;
        mb_term_t *roots[MB_REG_COUNT];
        uint8_t i;

        if (mb_fetch_u8(proc, &r_dst) != MB_OK ||
            mb_fetch_u8(proc, &r_head) != MB_OK ||
            mb_fetch_u8(proc, &r_tail) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(r_dst) || !mb_vm_valid_reg(r_head) || !mb_vm_valid_reg(r_tail)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        result = mb_heap_cons(&proc->heap, proc->regs[r_head], proc->regs[r_tail]);
        if (result == 0) {
            for (i = 0; i < MB_REG_COUNT; i++) roots[i] = &proc->regs[i];
            mb_heap_gc(&proc->heap, roots, MB_REG_COUNT);
            result = mb_heap_cons(&proc->heap, proc->regs[r_head], proc->regs[r_tail]);
            if (result == 0) {
                proc->last_error = MB_HEAP_OOM;
                return proc->last_error;
            }
        }
        proc->regs[r_dst] = result;
        return MB_OK;
    }

    case MB_OP_HEAD: {
        uint8_t r_dst, r_cons;
        mb_term_t *ptr;

        if (mb_fetch_u8(proc, &r_dst) != MB_OK ||
            mb_fetch_u8(proc, &r_cons) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(r_dst) || !mb_vm_valid_reg(r_cons)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        if (!MB_IS_CONS(proc->regs[r_cons])) {
            proc->last_error = MB_BAD_TERM;
            return proc->last_error;
        }
        ptr = &proc->heap.from[MB_GET_CONS(proc->regs[r_cons])];
        proc->regs[r_dst] = ptr[0];
        return MB_OK;
    }

    case MB_OP_TAIL: {
        uint8_t r_dst, r_cons;
        mb_term_t *ptr;

        if (mb_fetch_u8(proc, &r_dst) != MB_OK ||
            mb_fetch_u8(proc, &r_cons) != MB_OK) {
            proc->last_error = MB_EOF;
            return proc->last_error;
        }
        if (!mb_vm_valid_reg(r_dst) || !mb_vm_valid_reg(r_cons)) {
            proc->last_error = MB_BAD_REG;
            return proc->last_error;
        }
        if (!MB_IS_CONS(proc->regs[r_cons])) {
            proc->last_error = MB_BAD_TERM;
            return proc->last_error;
        }
        ptr = &proc->heap.from[MB_GET_CONS(proc->regs[r_cons])];
        proc->regs[r_dst] = ptr[1];
        return MB_OK;
    }

    case MB_OP_HALT:
        proc->halted = 1;
        proc->state = MB_PROC_HALTED;
        return MB_OK;

    default:
        proc->last_error = MB_BAD_OPCODE;
        return proc->last_error;
    }
}

int mb_proc_run(mb_process_t *proc, void *sched, uint32_t max_steps) {
    proc->reductions = 0;
    while (!proc->halted && proc->reductions < max_steps) {
        int rc = mb_proc_step(proc, sched);
        if (rc != MB_OK) {
            return rc;
        }
        /* In scheduler mode, stop if process yielded/blocked. */
        if (sched != NULL && proc->state != MB_PROC_READY) {
            break;
        }
        proc->reductions++;
    }
    return MB_OK;
}

int mb_vm_mailbox_push_proc(mb_process_t *proc, mb_command_t cmd) {
    return mb_mailbox_push_raw(&proc->mailbox, cmd);
}

/* --- mb_vm_t compatibility layer --- */

static void mb_vm_to_proc(const mb_vm_t *vm, mb_process_t *proc) {
    memset(proc, 0, sizeof(*proc));
    proc->pid = MB_PID_NONE;
    proc->state = vm->halted ? MB_PROC_HALTED : MB_PROC_READY;
    proc->program = vm->program;
    proc->program_size = vm->program_size;
    proc->pc = vm->pc;
    memcpy(proc->regs, vm->regs, sizeof(proc->regs));
    proc->mailbox = vm->mailbox;
    proc->halted = vm->halted;
    proc->last_error = vm->last_error;
}

static void mb_proc_to_vm(const mb_process_t *proc, mb_vm_t *vm) {
    vm->pc = proc->pc;
    memcpy(vm->regs, proc->regs, sizeof(vm->regs));
    vm->mailbox = proc->mailbox;
    vm->halted = proc->halted;
    vm->last_error = proc->last_error;
}

void mb_vm_init(mb_vm_t *vm, const uint8_t *program, size_t program_size) {
    memset(vm, 0, sizeof(*vm));
    vm->program = program;
    vm->program_size = program_size;
}

int mb_vm_mailbox_push(mb_vm_t *vm, mb_command_t cmd) {
    return mb_mailbox_push_raw(&vm->mailbox, cmd);
}

int mb_vm_mailbox_pop(mb_vm_t *vm, mb_command_t *cmd) {
    return mb_mailbox_pop_raw(&vm->mailbox, cmd);
}

int mb_vm_step(mb_vm_t *vm) {
    mb_process_t proc;
    int rc;
    mb_vm_to_proc(vm, &proc);
    rc = mb_proc_step(&proc, NULL);
    mb_proc_to_vm(&proc, vm);
    return rc;
}

int mb_vm_run(mb_vm_t *vm, uint32_t max_steps) {
    mb_process_t proc;
    int rc;
    mb_vm_to_proc(vm, &proc);
    rc = mb_proc_run(&proc, NULL, max_steps);
    mb_proc_to_vm(&proc, vm);
    return rc;
}
