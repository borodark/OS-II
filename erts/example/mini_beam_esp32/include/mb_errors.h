#ifndef MB_ERRORS_H
#define MB_ERRORS_H

typedef enum {
    MB_OK = 0,
    MB_EOF = 1,
    MB_BAD_REG = 2,
    MB_BAD_OPCODE = 3,
    MB_BAD_BIF = 4,
    MB_BAD_ARGC = 5,
    MB_MAILBOX_EMPTY = 6,
    MB_INVALID_COMMAND = 7,
    MB_BAD_ARGUMENT = 8,
    MB_MAILBOX_FULL = 9,
    MB_SCHED_IDLE = 10,
    MB_BAD_PID = 11,
    MB_PROC_TABLE_FULL = 12,
    MB_HEAP_OOM = 13,
    MB_BAD_TERM = 14,
    MB_BAD_ARITY = 15
} mb_status_t;

#endif
