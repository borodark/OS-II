/**
 * M3.6 long-run stability test.
 *
 * Two processes run for 100K+ scheduler ticks:
 *
 * Process A ("producer"):
 *   Loop: allocate a 2-tuple {counter, counter+1}, SEND as I2C_READ
 *   command with counter as the sensor_id, YIELD, repeat.
 *
 * Process B ("consumer"):
 *   Loop: RECV_CMD, allocate a cons cell [value | acc], YIELD, repeat.
 *
 * Assertions:
 *   - No MB_HEAP_OOM (GC reclaims dead data each cycle)
 *   - heap.hp stays bounded (never exceeds capacity)
 *   - gc_count grows (GC is running)
 *   - No VM errors
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

#define STABILITY_TICKS  200000

static int failures = 0;

static void check_int(const char *name, int expected, int actual) {
    if (expected != actual) {
        fprintf(stderr, "FAIL %s: expected=%d actual=%d\n", name, expected, actual);
        failures++;
    }
}

/*
 * Producer program (process A):
 *   r0 = counter (starts 0)
 *   r1 = 1 (increment)
 *   r2 = PID of consumer (2)
 *   r3 = MB_CMD_I2C_READ (3)
 *   r4 = 0 (bus)
 *   r5 = 0x39 (addr)
 *
 * loop:
 *   r6 = r0 + r1           ; counter + 1
 *   MAKE_TUPLE r7, 2, r0, r6  ; {counter, counter+1}
 *   SEND r2, r3, r4, r5, r4, r0  ; send I2C_READ cmd to B, d=counter
 *   r0 = r0 + r1           ; counter++
 *   YIELD
 *   JMP loop
 */
static const uint8_t prog_producer[] = {
    /* r0 = 0 (counter) */
    MB_OP_CONST_I32, 0, I32LE(0),
    /* r1 = 1 */
    MB_OP_CONST_I32, 1, I32LE(1),
    /* r2 = 2 (consumer PID) */
    MB_OP_CONST_I32, 2, I32LE(2),
    /* r3 = MB_CMD_I2C_READ = 3 */
    MB_OP_CONST_I32, 3, I32LE(3),
    /* r4 = 0 (bus) */
    MB_OP_CONST_I32, 4, I32LE(0),
    /* r5 = 0x39 (addr) */
    MB_OP_CONST_I32, 5, I32LE(0x39),

    /* loop (pc=36): */
    /* r6 = r0 + r1 */
    MB_OP_ADD, 6, 0, 1,
    /* MAKE_TUPLE r7, arity=2, r0, r6 */
    MB_OP_MAKE_TUPLE, 7, 2, 0, 6,
    /* SEND r2, r3, r4, r5, r4, r0 */
    /*   target=r2(pid 2), type=r3(I2C_READ), a=r4(bus), b=r5(addr), c=r4(0), d=r0(counter) */
    MB_OP_SEND, 2, 3, 4, 5, 4, 0,
    /* r0 = r0 + r1 (counter++) */
    MB_OP_ADD, 0, 0, 1,
    /* YIELD */
    MB_OP_YIELD,
    /* JMP back to loop (calculate offset) */
    /* Current pc after JMP opcode+i32 = 36+4+5+7+4+1+5 = 62, loop_pc = 36 */
    /* offset = 36 - (pc after reading offset) */
    MB_OP_JMP, I32LE(-26)
};

/*
 * Consumer program (process B):
 *   r1 = 0 (processed count)
 *   r2 = 1 (increment)
 *
 * loop:
 *   RECV_CMD r3, r4, r5, r6, r7   ; blocks until message
 *   r1 = r1 + r2                  ; processed++
 *   MAKE_TUPLE r0, 2, r1, r7      ; r0 = {count, value}
 *     -> overwrites previous r0, old tuple becomes garbage for GC
 *   YIELD
 *   JMP loop
 */
static const uint8_t prog_consumer[] = {
    /* r2 = 1 */
    MB_OP_CONST_I32, 2, I32LE(1),

    /* loop (pc=6): */
    /* RECV_CMD -> r3=type, r4=a, r5=b, r6=c, r7=d */
    MB_OP_RECV_CMD, 3, 4, 5, 6, 7,
    /* r1 = r1 + r2 ; processed++ */
    MB_OP_ADD, 1, 1, 2,
    /* MAKE_TUPLE r0, arity=2, r1, r7 */
    MB_OP_MAKE_TUPLE, 0, 2, 1, 7,
    /* YIELD */
    MB_OP_YIELD,
    /* JMP loop */
    MB_OP_JMP, I32LE(-15)
};

int main(void) {
    mb_scheduler_t sched;
    mb_pid_t pid_a, pid_b;
    mb_process_t *pa, *pb;
    int rc;
    int ticks = 0;
    int idle_count = 0;

    mb_sched_init(&sched);
    pid_a = mb_sched_spawn(&sched, prog_producer, sizeof(prog_producer));
    pid_b = mb_sched_spawn(&sched, prog_consumer, sizeof(prog_consumer));

    check_int("spawn_a", 1, pid_a);
    check_int("spawn_b", 2, pid_b);

    pa = mb_sched_proc(&sched, pid_a);
    pb = mb_sched_proc(&sched, pid_b);

    while (ticks < STABILITY_TICKS) {
        rc = mb_sched_tick(&sched);
        if (rc == MB_SCHED_IDLE) {
            idle_count++;
            if (idle_count > 100) {
                fprintf(stderr, "stuck idle at tick %d\n", ticks);
                break;
            }
            continue;
        }
        idle_count = 0;
        if (rc != MB_OK) {
            fprintf(stderr, "FAIL: vm error rc=%d at tick %d, "
                    "pa: state=%d last_err=%d pc=%zu hp=%zu gc=%u, "
                    "pb: state=%d last_err=%d pc=%zu hp=%zu gc=%u\n",
                    rc, ticks,
                    pa->state, pa->last_error, pa->pc,
                    pa->heap.hp, pa->heap.gc_count,
                    pb->state, pb->last_error, pb->pc,
                    pb->heap.hp, pb->heap.gc_count);
            failures++;
            break;
        }
        ticks++;

        /* Periodic heap bounds check */
        if ((ticks % 10000) == 0) {
            if (pa->heap.hp > pa->heap.capacity ||
                pb->heap.hp > pb->heap.capacity) {
                fprintf(stderr, "FAIL: heap overflow at tick %d "
                        "pa.hp=%zu/%zu pb.hp=%zu/%zu\n",
                        ticks,
                        pa->heap.hp, pa->heap.capacity,
                        pb->heap.hp, pb->heap.capacity);
                failures++;
                break;
            }
        }
    }

    check_int("enough_ticks", 1, ticks >= STABILITY_TICKS);

    /* GC must have run (both processes allocate every cycle) */
    check_int("pa_gc_ran", 1, pa->heap.gc_count > 0);
    check_int("pb_gc_ran", 1, pb->heap.gc_count > 0);

    /* Heaps must be within bounds */
    check_int("pa_hp_bounded", 1, pa->heap.hp <= pa->heap.capacity);
    check_int("pb_hp_bounded", 1, pb->heap.hp <= pb->heap.capacity);

    /* Producer counter should have advanced significantly */
    check_int("pa_counter_positive", 1, MB_GET_SMALLINT(pa->regs[0]) > 1000);

    /* Consumer should have processed many messages */
    check_int("pb_processed_positive", 1, MB_GET_SMALLINT(pb->regs[1]) > 1000);

    if (failures != 0) {
        fprintf(stderr, "stability failures=%d\n", failures);
        return 1;
    }

    printf("mini_beam_host_stability: %d ticks, "
           "producer counter=%d gc=%u, consumer processed=%d gc=%u — PASS\n",
           ticks,
           MB_GET_SMALLINT(pa->regs[0]), pa->heap.gc_count,
           MB_GET_SMALLINT(pb->regs[1]), pb->heap.gc_count);
    return 0;
}
