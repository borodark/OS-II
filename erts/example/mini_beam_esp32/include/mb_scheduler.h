#ifndef MB_SCHEDULER_H
#define MB_SCHEDULER_H

/**
 * @file mb_scheduler.h
 * @brief Cooperative round-robin scheduler for lightweight processes.
 *
 * The scheduler owns a fixed-size process table and dispatches processes
 * in round-robin order.  Each process runs for up to MB_REDUCTIONS
 * instructions before yielding.  Processes transition to WAITING when
 * RECV_CMD finds an empty mailbox (woken on message arrival) or to
 * SLEEPING when SLEEP_MS is executed (woken when monotonic time passes
 * the deadline).
 */

#include "mb_process.h"
#include "mb_types.h"

typedef struct mb_scheduler_s {
    mb_process_t procs[MB_MAX_PROCESSES];
    uint8_t      current;
    uint8_t      count;
} mb_scheduler_t;

/**
 * @brief Initialize the scheduler (all slots free).
 */
void mb_sched_init(mb_scheduler_t *sched);

/**
 * @brief Spawn a new process running the given bytecode.
 *
 * @return PID (1..MB_MAX_PROCESSES) on success, MB_PID_NONE if table full.
 */
mb_pid_t mb_sched_spawn(mb_scheduler_t *sched,
                        const uint8_t *program, size_t program_size);

/**
 * @brief Run one scheduling round: pick a runnable process, execute up to
 *        MB_REDUCTIONS instructions.
 *
 * @return MB_OK if a process ran, MB_SCHED_IDLE if no process is runnable,
 *         or a VM error code if a process faulted.
 */
int mb_sched_tick(mb_scheduler_t *sched);

/**
 * @brief Push a command into a process's mailbox from external code.
 *
 * Wakes the target process if it was in WAITING state.
 *
 * @return MB_OK, MB_MAILBOX_FULL, MB_BAD_PID, or validation error.
 */
int mb_sched_send(mb_scheduler_t *sched, mb_pid_t dst, mb_command_t cmd);

/**
 * @brief Look up a process by PID.
 *
 * @return Pointer to process, or NULL if PID is invalid or slot is free.
 */
mb_process_t *mb_sched_proc(mb_scheduler_t *sched, mb_pid_t pid);

#endif
