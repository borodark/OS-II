/**
 * M2.4 proof: two processes coordinate I2C-to-PWM flow via scheduler.
 *
 * Process A ("sensor"):
 *   1. Load I2C params into registers
 *   2. Call I2C_READ_REG BIF -> value in r7
 *   3. Pack a PWM_SET_DUTY command with the read value as duty permille
 *   4. SEND command to process B
 *   5. HALT
 *
 * Process B ("actuator"):
 *   1. RECV_CMD (blocks until A sends)
 *   2. Call PWM_SET_DUTY BIF with received params
 *   3. HALT
 *
 * Expected: stub HAL produces i2c_read value = (0x68 ^ 0x75 ^ 0) = 0x1d = 29
 * Process A sends PWM_SET_DUTY(channel=0, permille=29) to B.
 * Process B executes PWM_SET_DUTY(0, 29).
 */

#include <stdio.h>
#include <stdlib.h>

#include "mb_vm.h"
#include "mb_scheduler.h"

#define I32LE(v) \
    (uint8_t)((v) & 0xff), \
    (uint8_t)(((v) >> 8) & 0xff), \
    (uint8_t)(((v) >> 16) & 0xff), \
    (uint8_t)(((v) >> 24) & 0xff)

static int failures = 0;

static void check_int(const char *name, int expected, int actual) {
    if (expected != actual) {
        fprintf(stderr, "FAIL %s: expected=%d actual=%d\n", name, expected, actual);
        failures++;
    }
}

/*
 * Program A (sensor process):
 *   r0 = I2C bus (0)
 *   r1 = I2C addr (0x68)
 *   r2 = I2C reg (0x75)
 *   r7 = i2c_read_reg result (value or error)
 *
 *   r3 = target PID (2, process B)
 *   r4 = cmd type (MB_CMD_PWM_SET_DUTY = 2)
 *   r5 = channel (0)
 *   r6 = duty_permille = r7 (read value)
 *   r8 = 0 (unused cmd fields c, d)
 *
 *   SEND r3 r4 r5 r6 r8 r8  -> send PWM cmd to B
 *   HALT
 */
static const uint8_t prog_sensor[] = {
    /* r0=0 (bus) */
    MB_OP_CONST_I32, 0, I32LE(0),
    /* r1=0x68 (addr) */
    MB_OP_CONST_I32, 1, I32LE(0x68),
    /* r2=0x75 (reg) */
    MB_OP_CONST_I32, 2, I32LE(0x75),

    /* r7 = i2c_read_reg(r0, r1, r2) */
    MB_OP_CALL_BIF, MB_BIF_I2C_READ_REG, 3, 0, 1, 2, 7,

    /* r3 = 2 (PID of actuator process) */
    MB_OP_CONST_I32, 3, I32LE(2),
    /* r4 = MB_CMD_PWM_SET_DUTY (2) */
    MB_OP_CONST_I32, 4, I32LE(2),
    /* r5 = 0 (channel) */
    MB_OP_CONST_I32, 5, I32LE(0),
    /* r6 = r7 (copy read value as duty permille) */
    MB_OP_MOVE, 6, 7, 0,
    /* r8 = 0 (unused fields) */
    MB_OP_CONST_I32, 8, I32LE(0),

    /* SEND: target=r3, cmd={r4, r5, r6, r8, r8} */
    MB_OP_SEND, 3, 4, 5, 6, 8, 8,

    MB_OP_HALT
};

/*
 * Program B (actuator process):
 *   RECV_CMD -> r0=type, r1=channel, r2=permille, r3=c, r4=d
 *   CALL_BIF PWM_SET_DUTY(r1, r2) -> r5
 *   HALT
 */
static const uint8_t prog_actuator[] = {
    /* RECV_CMD: r0=type r1=a r2=b r3=c r4=d */
    MB_OP_RECV_CMD, 0, 1, 2, 3, 4,

    /* r5 = pwm_set_duty(r1=channel, r2=permille) */
    MB_OP_CALL_BIF, MB_BIF_PWM_SET_DUTY, 2, 1, 2, 5,

    MB_OP_HALT
};

int main(void) {
    mb_scheduler_t sched;
    mb_pid_t pid_a, pid_b;
    mb_process_t *proc_a, *proc_b;
    int ticks = 0;
    int rc;

    mb_sched_init(&sched);

    pid_a = mb_sched_spawn(&sched, prog_sensor, sizeof(prog_sensor));
    pid_b = mb_sched_spawn(&sched, prog_actuator, sizeof(prog_actuator));

    check_int("pid_a", 1, pid_a);
    check_int("pid_b", 2, pid_b);

    /* Run scheduler ticks until both processes halt (max 100 to prevent hangs) */
    while (ticks < 100) {
        rc = mb_sched_tick(&sched);
        if (rc == MB_SCHED_IDLE) {
            break;
        }
        if (rc != MB_OK) {
            fprintf(stderr, "scheduler error rc=%d at tick %d\n", rc, ticks);
            return 1;
        }
        ticks++;
    }

    proc_a = mb_sched_proc(&sched, pid_a);
    proc_b = mb_sched_proc(&sched, pid_b);

    /* Process A should have halted after I2C read + SEND */
    check_int("proc_a_halted", MB_PROC_HALTED, proc_a->state);
    /* Stub I2C read returns (addr ^ reg ^ bus) = 0x68 ^ 0x75 ^ 0 = 0x1d = 29 */
    check_int("proc_a_i2c_value", 29, MB_GET_SMALLINT(proc_a->regs[7]));
    /* SEND result in r3 should be MB_OK */
    check_int("proc_a_send_rc", MB_OK, MB_GET_SMALLINT(proc_a->regs[3]));

    /* Process B should have received the command and executed PWM */
    check_int("proc_b_halted", MB_PROC_HALTED, proc_b->state);
    check_int("proc_b_cmd_type", MB_CMD_PWM_SET_DUTY, MB_GET_SMALLINT(proc_b->regs[0]));
    check_int("proc_b_channel", 0, MB_GET_SMALLINT(proc_b->regs[1]));
    check_int("proc_b_permille", 29, MB_GET_SMALLINT(proc_b->regs[2]));
    /* PWM BIF return code */
    check_int("proc_b_pwm_rc", 0, MB_GET_SMALLINT(proc_b->regs[5]));

    if (failures != 0) {
        fprintf(stderr, "multiproc failures=%d\n", failures);
        return 1;
    }

    printf("mini_beam_host_multiproc: two-process I2C-to-PWM proof passed (%d scheduler ticks)\n", ticks);
    return 0;
}
