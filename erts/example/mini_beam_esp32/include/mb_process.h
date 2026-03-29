#ifndef MB_PROCESS_H
#define MB_PROCESS_H

/**
 * @file mb_process.h
 * @brief Lightweight process structure for cooperative multi-process scheduling.
 *
 * Each process owns its own register file, program counter, and mailbox.
 * The scheduler (mb_scheduler.h) manages process state transitions and
 * round-robin dispatch.
 */

#include "mb_heap.h"
#include "mb_types.h"

#define MB_MAX_PROCESSES 8
#define MB_PID_NONE      0
#define MB_REDUCTIONS    64

typedef uint8_t mb_pid_t;

typedef enum {
    MB_PROC_FREE     = 0,
    MB_PROC_READY    = 1,
    MB_PROC_WAITING  = 2,
    MB_PROC_SLEEPING = 3,
    MB_PROC_HALTED   = 4
} mb_proc_state_t;

typedef struct {
    mb_pid_t          pid;
    mb_proc_state_t   state;
    const uint8_t    *program;
    size_t            program_size;
    size_t            pc;
    mb_term_t         regs[MB_REG_COUNT];
    mb_mailbox_t      mailbox;
    int               halted;
    int               last_error;
    mb_heap_t         heap;
    uint32_t          sleep_until_ms;
    uint32_t          reductions;
} mb_process_t;

/**
 * @brief Initialize a process with a bytecode program.
 */
void mb_proc_init(mb_process_t *proc, mb_pid_t pid,
                  const uint8_t *program, size_t program_size);

/**
 * @brief Execute one instruction on a process.
 *
 * @param proc Process to step.
 * @param sched Scheduler context (NULL for single-process compat mode).
 * @return MB_OK on success, otherwise a VM error code.
 */
int mb_proc_step(mb_process_t *proc, void *sched);

/**
 * @brief Execute up to max_steps instructions on a process.
 *
 * @param proc Process to run.
 * @param sched Scheduler context (NULL for single-process compat mode).
 * @param max_steps Maximum instruction count.
 * @return MB_OK on success, otherwise a VM error code.
 */
int mb_proc_run(mb_process_t *proc, void *sched, uint32_t max_steps);

/**
 * @brief Push a validated command into a process's mailbox.
 */
int mb_vm_mailbox_push_proc(mb_process_t *proc, mb_command_t cmd);

#endif
