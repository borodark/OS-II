#include <stdint.h>

#include "esp_log.h"

#include "mb_vm.h"

#define I32LE(v) \
    (uint8_t)((v) & 0xff), \
    (uint8_t)(((v) >> 8) & 0xff), \
    (uint8_t)(((v) >> 16) & 0xff), \
    (uint8_t)(((v) >> 24) & 0xff)

static const char *TAG = "mini_beam_esp32";

static const uint8_t app_program[] = {
    MB_OP_RECV_CMD, 0, 1, 2, 3, 4,

    MB_OP_CONST_I32, 5, I32LE(MB_CMD_GPIO_WRITE),
    MB_OP_SUB, 6, 0, 5,
    MB_OP_JMP_IF_ZERO, 6, I32LE(21),

    MB_OP_CONST_I32, 5, I32LE(MB_CMD_PWM_SET_DUTY),
    MB_OP_SUB, 6, 0, 5,
    MB_OP_JMP_IF_ZERO, 6, I32LE(15),

    MB_OP_CONST_I32, 5, I32LE(MB_CMD_I2C_READ),
    MB_OP_SUB, 6, 0, 5,
    MB_OP_JMP_IF_ZERO, 6, I32LE(9),

    MB_OP_HALT,

    MB_OP_CALL_BIF, MB_BIF_GPIO_WRITE, 2, 1, 2, 15,
    MB_OP_HALT,

    MB_OP_CALL_BIF, MB_BIF_PWM_SET_DUTY, 2, 1, 2, 15,
    MB_OP_HALT,

    MB_OP_CALL_BIF, MB_BIF_I2C_READ_REG, 3, 1, 2, 3, 7,
    MB_OP_HALT
};

void app_main(void) {
    mb_vm_t vm;
    mb_command_t cmd;
    int rc;

    mb_vm_init(&vm, app_program, sizeof(app_program));

    cmd.type = MB_CMD_GPIO_WRITE;
    cmd.a = 2;
    cmd.b = 1;
    cmd.c = 0;
    cmd.d = 0;
    rc = mb_vm_mailbox_push(&vm, cmd);
    if (rc != MB_OK) {
        ESP_LOGE(TAG, "mailbox push failed: %d", rc);
        return;
    }

    rc = mb_vm_run(&vm, 128);
    if (rc != MB_OK) {
        ESP_LOGE(TAG, "vm failed: err=%d pc=%u", rc, (unsigned)vm.pc);
        return;
    }

    ESP_LOGI(TAG, "vm done: last_error=%d", vm.last_error);
}
