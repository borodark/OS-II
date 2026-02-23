#include <stdio.h>

#include "mb_vm.h"

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

static void test_invalid_command_rejected(void) {
    mb_vm_t vm;
    mb_command_t cmd = {0};

    mb_vm_init(&vm, NULL, 0);
    cmd.type = 999;
    check_int("invalid_command", MB_INVALID_COMMAND, mb_vm_mailbox_push(&vm, cmd));
}

static void test_bad_argument_rejected(void) {
    mb_vm_t vm;
    mb_command_t cmd = {0};

    mb_vm_init(&vm, NULL, 0);
    cmd.type = MB_CMD_GPIO_WRITE;
    cmd.a = 100;
    cmd.b = 1;
    check_int("bad_arg", MB_BAD_ARGUMENT, mb_vm_mailbox_push(&vm, cmd));
}

static void test_mailbox_full(void) {
    mb_vm_t vm;
    mb_command_t cmd = {0};
    int i;

    mb_vm_init(&vm, NULL, 0);
    cmd.type = MB_CMD_GPIO_WRITE;
    cmd.a = 2;
    cmd.b = 1;

    for (i = 0; i < MB_MAILBOX_CAPACITY; i++) {
        check_int("mailbox_push_ok", MB_OK, mb_vm_mailbox_push(&vm, cmd));
    }
    check_int("mailbox_full", MB_MAILBOX_FULL, mb_vm_mailbox_push(&vm, cmd));
}

static void test_invalid_opcode(void) {
    mb_vm_t vm;
    static const uint8_t program[] = {0x7E};
    mb_vm_init(&vm, program, sizeof(program));
    check_int("invalid_opcode", MB_BAD_OPCODE, mb_vm_run(&vm, 8));
}

static void test_bad_register_decode(void) {
    mb_vm_t vm;
    static const uint8_t program[] = {
        MB_OP_CONST_I32, 77, I32LE(1)
    };
    mb_vm_init(&vm, program, sizeof(program));
    check_int("bad_reg", MB_BAD_REG, mb_vm_run(&vm, 8));
}

static void test_recv_empty_is_nonfatal(void) {
    mb_vm_t vm;
    static const uint8_t program[] = {
        MB_OP_RECV_CMD, 0, 1, 2, 3, 4,
        MB_OP_HALT
    };

    mb_vm_init(&vm, program, sizeof(program));
    check_int("recv_empty_run", MB_OK, mb_vm_run(&vm, 8));
    check_int("recv_empty_type", MB_CMD_NONE, vm.regs[0]);
    check_int("recv_empty_status", MB_MAILBOX_EMPTY, vm.regs[1]);
}

int main(void) {
    test_invalid_command_rejected();
    test_bad_argument_rejected();
    test_mailbox_full();
    test_invalid_opcode();
    test_bad_register_decode();
    test_recv_empty_is_nonfatal();

    if (failures != 0) {
        fprintf(stderr, "regression failures=%d\n", failures);
        return 1;
    }

    printf("mini_beam_host_regression: all tests passed\n");
    return 0;
}
