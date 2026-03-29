#include "mb_scheduler.h"

#include <string.h>

#include "mb_errors.h"
#include "mb_hal.h"

void mb_sched_init(mb_scheduler_t *sched) {
    memset(sched, 0, sizeof(*sched));
}

mb_pid_t mb_sched_spawn(mb_scheduler_t *sched,
                        const uint8_t *program, size_t program_size) {
    uint8_t i;
    for (i = 0; i < MB_MAX_PROCESSES; i++) {
        if (sched->procs[i].state == MB_PROC_FREE) {
            mb_proc_init(&sched->procs[i], (mb_pid_t)(i + 1), program, program_size);
            sched->count++;
            return sched->procs[i].pid;
        }
    }
    return MB_PID_NONE;
}

mb_process_t *mb_sched_proc(mb_scheduler_t *sched, mb_pid_t pid) {
    if (pid == MB_PID_NONE || pid > MB_MAX_PROCESSES) {
        return NULL;
    }
    if (sched->procs[pid - 1].state == MB_PROC_FREE) {
        return NULL;
    }
    return &sched->procs[pid - 1];
}

int mb_sched_send(mb_scheduler_t *sched, mb_pid_t dst, mb_command_t cmd) {
    mb_process_t *proc = mb_sched_proc(sched, dst);
    int rc;

    if (proc == NULL) {
        return MB_BAD_PID;
    }

    rc = mb_vm_mailbox_push_proc(proc, cmd);
    if (rc == MB_OK && proc->state == MB_PROC_WAITING) {
        proc->state = MB_PROC_READY;
    }
    return rc;
}

static void mb_sched_wake_sleepers(mb_scheduler_t *sched) {
    uint8_t i;
    uint32_t now = mb_hal_monotonic_ms();
    for (i = 0; i < MB_MAX_PROCESSES; i++) {
        mb_process_t *p = &sched->procs[i];
        if (p->state == MB_PROC_SLEEPING && now >= p->sleep_until_ms) {
            p->state = MB_PROC_READY;
        }
    }
}

int mb_sched_tick(mb_scheduler_t *sched) {
    uint8_t i, start;
    mb_process_t *proc = NULL;

    mb_sched_wake_sleepers(sched);

    /* Round-robin scan for a READY process starting after current */
    start = sched->current;
    for (i = 0; i < MB_MAX_PROCESSES; i++) {
        uint8_t idx = (start + i) % MB_MAX_PROCESSES;
        if (sched->procs[idx].state == MB_PROC_READY) {
            proc = &sched->procs[idx];
            sched->current = (idx + 1) % MB_MAX_PROCESSES;
            break;
        }
    }

    if (proc == NULL) {
        return MB_SCHED_IDLE;
    }

    return mb_proc_run(proc, sched, MB_REDUCTIONS);
}
