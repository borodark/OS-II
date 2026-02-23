#include <stdio.h>

#include "mb_vm.h"

static const uint8_t mailbox_program[] = {
    MB_OP_RECV_CMD, 0, 1, 2, 3, 4,
    MB_OP_HALT
};

int main(void) {
    mb_vm_t vm;
    mb_command_t cmd;
    int rc;

    mb_vm_init(&vm, mailbox_program, sizeof(mailbox_program));

    cmd.type = MB_CMD_I2C_WRITE;
    cmd.a = 0;
    cmd.b = 0x68;
    cmd.c = 0x75;
    cmd.d = 0x1C;

    rc = mb_vm_mailbox_push(&vm, cmd);
    if (rc != MB_OK) {
        fprintf(stderr, "mailbox push failed: %d\n", rc);
        return 1;
    }

    rc = mb_vm_run(&vm, 8);
    if (rc != MB_OK) {
        fprintf(stderr, "vm failed: err=%d pc=%zu\n", vm.last_error, vm.pc);
        return 1;
    }

    printf("mailbox recv: type=%d a=%d b=%d c=%d d=%d\n",
           vm.regs[0], vm.regs[1], vm.regs[2], vm.regs[3], vm.regs[4]);

    cmd.type = 999;
    cmd.a = 0;
    cmd.b = 0;
    cmd.c = 0;
    cmd.d = 0;
    rc = mb_vm_mailbox_push(&vm, cmd);
    printf("invalid command push status=%d (expected %d)\n", rc, MB_INVALID_COMMAND);

    return 0;
}
