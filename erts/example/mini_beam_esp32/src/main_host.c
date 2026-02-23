#include <stdio.h>

#include "mb_vm.h"

#define I32LE(v) \
    (uint8_t)((v) & 0xff), \
    (uint8_t)(((v) >> 8) & 0xff), \
    (uint8_t)(((v) >> 16) & 0xff), \
    (uint8_t)(((v) >> 24) & 0xff)

static const uint8_t demo_program[] = {
    /* r0=2, r1=1 -> gpio_write */
    MB_OP_CONST_I32, 0, I32LE(2),
    MB_OP_CONST_I32, 1, I32LE(1),
    MB_OP_CALL_BIF, MB_BIF_GPIO_WRITE, 2, 0, 1, 15,

    /* r2=0, r3=600 -> pwm_set_duty */
    MB_OP_CONST_I32, 2, I32LE(0),
    MB_OP_CONST_I32, 3, I32LE(600),
    MB_OP_CALL_BIF, MB_BIF_PWM_SET_DUTY, 2, 2, 3, 15,

    /* r4=0, r5=0x68, r6=0x75 -> i2c_read */
    MB_OP_CONST_I32, 4, I32LE(0),
    MB_OP_CONST_I32, 5, I32LE(0x68),
    MB_OP_CONST_I32, 6, I32LE(0x75),
    MB_OP_CALL_BIF, MB_BIF_I2C_READ_REG, 3, 4, 5, 6, 7,

    /* r9=2 -> gpio_read */
    MB_OP_CONST_I32, 9, I32LE(2),
    MB_OP_CALL_BIF, MB_BIF_GPIO_READ, 1, 9, 10,

    /* r2=0, r11=1000 -> pwm_config */
    MB_OP_CONST_I32, 11, I32LE(1000),
    MB_OP_CALL_BIF, MB_BIF_PWM_CONFIG, 2, 2, 11, 15,

    /* r12=0x1C -> i2c_write(bus=0, addr=0x68, reg=0x75, value=0x1C) */
    MB_OP_CONST_I32, 12, I32LE(0x1C),
    MB_OP_CALL_BIF, MB_BIF_I2C_WRITE_REG, 4, 4, 5, 6, 12, 15,

    /* r8=50 -> sleep */
    MB_OP_CONST_I32, 8, I32LE(50),
    MB_OP_SLEEP_MS, 8,

    MB_OP_HALT
};

int main(void) {
    mb_vm_t vm;
    mb_vm_init(&vm, demo_program, sizeof(demo_program));

    if (mb_vm_run(&vm, 1024) != 0) {
        fprintf(stderr, "vm failed: err=%d pc=%zu\n", vm.last_error, vm.pc);
        return 1;
    }

    printf("vm done: i2c_value_reg7=%d gpio_level_reg10=%d\n", vm.regs[7], vm.regs[10]);
    return 0;
}
