#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <hal/nrf_clock.h>

#if defined(CONFIG_USB_DEVICE_STACK)
#include <zephyr/usb/usb_device.h>
#endif

#include "mb_vm.h"

LOG_MODULE_REGISTER(mini_beam_nrf52, LOG_LEVEL_INF);

#if DT_NODE_HAS_STATUS(DT_ALIAS(i2c0), okay)
#define OS2_I2C0_NODE DT_ALIAS(i2c0)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
#define OS2_I2C0_NODE DT_NODELABEL(i2c0)
#endif

#if defined(OS2_I2C0_NODE)
static const struct device *const os2_i2c0 = DEVICE_DT_GET(OS2_I2C0_NODE);
#define OS2_HAS_I2C0 1
#else
#define OS2_HAS_I2C0 0
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
static const struct device *const os2_i2c1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));
#define OS2_HAS_I2C1 1
#else
#define OS2_HAS_I2C1 0
#endif

#if DT_NODE_EXISTS(DT_PATH(zephyr_user)) && DT_NODE_HAS_PROP(DT_PATH(zephyr_user), pull_up_gpios)
static const struct gpio_dt_spec os2_i2c_pullup =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), pull_up_gpios);
#define OS2_HAS_I2C_PULLUP 1
#else
#define OS2_HAS_I2C_PULLUP 0
#endif

#if DT_NODE_EXISTS(DT_PATH(vdd_env)) && DT_NODE_HAS_PROP(DT_PATH(vdd_env), enable_gpios)
static const struct gpio_dt_spec os2_vdd_env_en =
    GPIO_DT_SPEC_GET(DT_PATH(vdd_env), enable_gpios);
#define OS2_HAS_VDD_ENV_EN 1
#else
#define OS2_HAS_VDD_ENV_EN 0
#endif

#if DT_NODE_EXISTS(DT_PATH(mic_pwr)) && DT_NODE_HAS_PROP(DT_PATH(mic_pwr), enable_gpios)
static const struct gpio_dt_spec os2_mic_pwr_en =
    GPIO_DT_SPEC_GET(DT_PATH(mic_pwr), enable_gpios);
#define OS2_HAS_MIC_PWR_EN 1
#else
#define OS2_HAS_MIC_PWR_EN 0
#endif

typedef struct {
    const char *name;
    uint8_t bus;
    uint8_t addr;
    uint8_t reg;
    uint8_t expected;
    uint8_t mask;
} os2_sensor_sig_t;

typedef struct {
    const char *name;
    uint8_t id;
    uint8_t bus;
    uint8_t addr;
    uint8_t reg;
} os2_sensor_target_t;

static const os2_sensor_sig_t os2_sensor_sigs[] = {
    {"MPU60x0/92x0", 0, 0x68, 0x75, 0x68, 0x7E},
    {"LSM9DS1_AG", 1, 0x6A, 0x0F, 0x68, 0xFF},
    {"LSM9DS1_MAG", 1, 0x1C, 0x0F, 0x3D, 0xFF},
    {"LSM6DS3/LSM6DSL", 1, 0x6A, 0x0F, 0x69, 0xFF},
    {"LSM6DSOX", 1, 0x6A, 0x0F, 0x6C, 0xFF},
    {"BMI270", 1, 0x68, 0x00, 0x24, 0xFF},
    {"BMM150", 1, 0x10, 0x40, 0x32, 0xFF},
    {"APDS9960", 1, 0x39, 0x92, 0xAB, 0xFF},
    {"HTS221", 1, 0x5F, 0x0F, 0xBC, 0xFF},
    {"LPS22HB", 1, 0x5C, 0x0F, 0xB1, 0xFF},
    {"BME280", 0, 0x76, 0xD0, 0x60, 0xFF},
    {"BME280", 0, 0x77, 0xD0, 0x60, 0xFF},
    {"BME280", 1, 0x76, 0xD0, 0x60, 0xFF},
    {"BME280", 1, 0x77, 0xD0, 0x60, 0xFF},
};

static uint8_t demo_program[128];

static void os2_emit_u8(size_t *pc, uint8_t value) {
    demo_program[(*pc)++] = value;
}

static void os2_emit_i32(size_t *pc, int32_t value) {
    demo_program[(*pc)++] = (uint8_t)(value & 0xff);
    demo_program[(*pc)++] = (uint8_t)((value >> 8) & 0xff);
    demo_program[(*pc)++] = (uint8_t)((value >> 16) & 0xff);
    demo_program[(*pc)++] = (uint8_t)((value >> 24) & 0xff);
}

static void os2_patch_rel_i32(size_t patch_pos, size_t target_pc) {
    int32_t rel = (int32_t)target_pc - (int32_t)(patch_pos + 4U);
    demo_program[patch_pos + 0] = (uint8_t)(rel & 0xff);
    demo_program[patch_pos + 1] = (uint8_t)((rel >> 8) & 0xff);
    demo_program[patch_pos + 2] = (uint8_t)((rel >> 16) & 0xff);
    demo_program[patch_pos + 3] = (uint8_t)((rel >> 24) & 0xff);
}

static size_t os2_build_cyclic_program(void) {
    size_t pc = 0;
    size_t loop_pc;
    size_t jz_to_sleep_patch;
    size_t jmp_to_loop_patch;

    /* r6 = idle sleep ms, r10 = published cmd type */
    os2_emit_u8(&pc, MB_OP_CONST_I32);
    os2_emit_u8(&pc, 6);
    os2_emit_i32(&pc, 20);

    os2_emit_u8(&pc, MB_OP_CONST_I32);
    os2_emit_u8(&pc, 10);
    os2_emit_i32(&pc, MB_CMD_NONE);

    loop_pc = pc;
    /* recv -> r0:type r1:bus r2:addr r3:reg r4:sensor_id */
    os2_emit_u8(&pc, MB_OP_RECV_CMD);
    os2_emit_u8(&pc, 0);
    os2_emit_u8(&pc, 1);
    os2_emit_u8(&pc, 2);
    os2_emit_u8(&pc, 3);
    os2_emit_u8(&pc, 4);

    /* if r0 == 0 then sleep path */
    os2_emit_u8(&pc, MB_OP_JMP_IF_ZERO);
    os2_emit_u8(&pc, 0);
    jz_to_sleep_patch = pc;
    os2_emit_i32(&pc, 0);

    /* value/err in r7 */
    os2_emit_u8(&pc, MB_OP_CALL_BIF);
    os2_emit_u8(&pc, MB_BIF_I2C_READ_REG);
    os2_emit_u8(&pc, 3);
    os2_emit_u8(&pc, 1);
    os2_emit_u8(&pc, 2);
    os2_emit_u8(&pc, 3);
    os2_emit_u8(&pc, 7);

    /* timestamp in r8 */
    os2_emit_u8(&pc, MB_OP_CALL_BIF);
    os2_emit_u8(&pc, MB_BIF_MONOTONIC_MS);
    os2_emit_u8(&pc, 0);
    os2_emit_u8(&pc, 8);

    /* copy sensor_id to r9 and cmd type to r10 (event marker) */
    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, 9);
    os2_emit_u8(&pc, 4);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, 10);
    os2_emit_u8(&pc, 0);
    os2_emit_u8(&pc, 0);

    /* jump back to loop */
    os2_emit_u8(&pc, MB_OP_JMP);
    jmp_to_loop_patch = pc;
    os2_emit_i32(&pc, 0);

    /* sleep path */
    os2_patch_rel_i32(jz_to_sleep_patch, pc);
    os2_emit_u8(&pc, MB_OP_SLEEP_MS);
    os2_emit_u8(&pc, 6);
    os2_emit_u8(&pc, MB_OP_JMP);
    os2_emit_i32(&pc, (int32_t)loop_pc - (int32_t)(pc + 4U));

    os2_patch_rel_i32(jmp_to_loop_patch, loop_pc);
    return pc;
}

static void os2_try_enable_i2c_pullups(void) {
    /* Match Arduino core init: detach SWO trace mux from the pull-up control pin. */
    NRF_CLOCK->TRACECONFIG = 0;

#if OS2_HAS_VDD_ENV_EN
    if (device_is_ready(os2_vdd_env_en.port) &&
        gpio_pin_configure_dt(&os2_vdd_env_en, GPIO_OUTPUT_ACTIVE) == 0) {
        LOG_INF("vdd_env enabled on pin %u", os2_vdd_env_en.pin);
    } else {
        LOG_WRN("failed to enable vdd_env");
    }
#endif

#if OS2_HAS_I2C_PULLUP
    if (!device_is_ready(os2_i2c_pullup.port)) {
        LOG_WRN("i2c pull-up gpio not ready");
        return;
    }
    if (gpio_pin_configure_dt(&os2_i2c_pullup, GPIO_OUTPUT_ACTIVE) != 0) {
        LOG_WRN("failed to enable i2c pull-up gpio");
        return;
    }
    LOG_INF("i2c pull-up enabled on pin %u", os2_i2c_pullup.pin);
#endif

#if OS2_HAS_MIC_PWR_EN
    if (device_is_ready(os2_mic_pwr_en.port) &&
        gpio_pin_configure_dt(&os2_mic_pwr_en, GPIO_OUTPUT_ACTIVE) == 0) {
        LOG_INF("mic_pwr enabled on pin %u", os2_mic_pwr_en.pin);
    }
#endif
}

static const struct device *os2_get_i2c_dev(uint8_t bus) {
    if (bus == 0U) {
#if OS2_HAS_I2C0
        if (device_is_ready(os2_i2c0)) {
            return os2_i2c0;
        }
#endif
        return NULL;
    }
    if (bus == 1U) {
#if OS2_HAS_I2C1
        if (device_is_ready(os2_i2c1)) {
            return os2_i2c1;
        }
#endif
        return NULL;
    }
    return NULL;
}

static size_t os2_i2c_signature_scan(os2_sensor_target_t *targets, size_t max_targets) {
    size_t i;
    size_t found = 0;
    uint8_t next_id = 1;

    LOG_INF("i2c signature scan start");
    for (i = 0; i < ARRAY_SIZE(os2_sensor_sigs); i++) {
        const os2_sensor_sig_t *sig = &os2_sensor_sigs[i];
        const struct device *dev = os2_get_i2c_dev(sig->bus);
        uint8_t value = 0;
        int rc;

        if (dev == NULL) {
            continue;
        }
        rc = i2c_write_read(dev, sig->addr, &sig->reg, 1U, &value, 1U);
        if (rc == 0) {
            LOG_INF("i2c bus%u probe %s addr=0x%02x reg=0x%02x val=0x%02x",
                sig->bus, sig->name, sig->addr, sig->reg, value);
            if (found < max_targets &&
                (value & sig->mask) == (sig->expected & sig->mask)) {
                targets[found].name = sig->name;
                targets[found].id = next_id++;
                targets[found].bus = sig->bus;
                targets[found].addr = sig->addr;
                targets[found].reg = sig->reg;
                found++;
            }
        }
    }
    if (found == 0) {
        LOG_WRN("i2c signature scan: no responders");
    }
    return found;
}

int main(void) {
    mb_vm_t vm;
    int rc;
    os2_sensor_target_t targets[6];
    size_t target_count = 0;
    uint32_t last_ts = 0;
    size_t i;
    size_t program_size;

#if defined(CONFIG_USB_DEVICE_STACK)
    {
        const struct device *const console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
        uint32_t dtr = 0;
        int wait_ms = 0;

        /* Bring up USB CDC ACM so logs are visible on /dev/ttyACM*. */
        (void)usb_enable(NULL);
        while (wait_ms < 3000U) {
            if (uart_line_ctrl_get(console_dev, UART_LINE_CTRL_DTR, &dtr) == 0 && dtr) {
                break;
            }
            k_msleep(100);
            wait_ms += 100;
        }
    }
#endif

    LOG_INF("mini_beam_nrf52 start");

    os2_try_enable_i2c_pullups();
    k_msleep(5);
    target_count = os2_i2c_signature_scan(targets, ARRAY_SIZE(targets));

    if (target_count == 0) {
        targets[0].name = "fallback";
        targets[0].id = 1;
        targets[0].bus = 0;
        targets[0].addr = 0x68;
        targets[0].reg = 0x75;
        target_count = 1;
        LOG_WRN("no known sensor signature found, using fallback target bus0 addr=0x68 reg=0x75");
    } else {
        for (i = 0; i < target_count; i++) {
            LOG_INF("target id=%u %s bus=%u addr=0x%02x reg=0x%02x",
                targets[i].id, targets[i].name, targets[i].bus, targets[i].addr, targets[i].reg);
        }
    }

    program_size = os2_build_cyclic_program();
    mb_vm_init(&vm, demo_program, program_size);

    while (1) {
        for (i = 0; i < target_count; i++) {
            mb_command_t cmd = {
                .type = MB_CMD_I2C_READ,
                .a = targets[i].bus,
                .b = targets[i].addr,
                .c = targets[i].reg,
                .d = targets[i].id
            };

            rc = mb_vm_mailbox_push(&vm, cmd);
            if (rc != MB_OK) {
                LOG_WRN("mailbox full/drop sensor_id=%u rc=%d", targets[i].id, rc);
                continue;
            }

            rc = mb_vm_run(&vm, 64);
            if (rc != MB_OK) {
                LOG_ERR("vm failed rc=%d last=%d pc=%u", rc, vm.last_error, (unsigned)vm.pc);
                return rc;
            }

            if (vm.regs[10] == MB_CMD_I2C_READ && (uint32_t)vm.regs[8] != last_ts) {
                const char *name = "unknown";
                size_t j;
                for (j = 0; j < target_count; j++) {
                    if ((uint8_t)vm.regs[9] == targets[j].id) {
                        name = targets[j].name;
                        break;
                    }
                }
                LOG_INF("event sensor_id=%d name=%s bus=%d addr=0x%02x reg=0x%02x value=%d ts=%u",
                    vm.regs[9], name, vm.regs[1], vm.regs[2], vm.regs[3], vm.regs[7], (uint32_t)vm.regs[8]);
                last_ts = (uint32_t)vm.regs[8];
                vm.regs[10] = MB_CMD_NONE;
            }
        }
        k_msleep(200);
    }

    return 0;
}
