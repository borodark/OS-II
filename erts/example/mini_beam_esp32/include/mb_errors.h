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
    MB_MAILBOX_FULL = 9
} mb_status_t;

#endif
