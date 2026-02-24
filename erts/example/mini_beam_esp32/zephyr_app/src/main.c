#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <hal/nrf_clock.h>
#include <hal/nrf_power.h>

#if defined(CONFIG_USB_DEVICE_STACK)
#include <zephyr/usb/usb_device.h>
#endif

#include "mb_vm.h"

/*
 * Module: OS/II Zephyr runtime (nRF52840 / Nano 33 BLE Sense path)
 *
 * Responsibilities:
 * - Bring up USB logging + optional sensor power rails and I2C pull-ups.
 * - Build/execute a small register-VM bytecode program that consumes mailbox
 *   commands and dispatches BIFs (I2C read, PWM set duty, monotonic timestamp).
 * - Periodically enqueue sensor read commands and PWM actuator commands.
 * - Emit stable event logs (sensor_event / actuator_event) using schema v1.
 * - Apply bounded-mailbox backpressure, retry/degraded policy, and watchdog
 *   recovery when degraded state persists beyond grace period.
 */

LOG_MODULE_REGISTER(mini_beam_nrf52, LOG_LEVEL_INF);

/* Locked event schema constants (v1). Keep in sync with contract doc. */
#define OS2_EVENT_SCHEMA_VERSION 1
#define OS2_EVENT_STATUS_OK 0
#define OS2_EVENT_STATUS_IO_ERROR 1
#define OS2_EVENT_STATUS_BAD_ARGUMENT 2
#define OS2_EVENT_STATUS_INTERNAL_ERROR 3
#define OS2_EVENT_STATUS_RETRYING 4
#define OS2_EVENT_STATUS_DEGRADED 5
#define OS2_EVENT_STATUS_RECOVERED 6

/* VM register mapping used by cyclic sensor/actuator routine. */
#define OS2_REG_CMD_TYPE 0
#define OS2_REG_BUS 1
#define OS2_REG_ADDR 2
#define OS2_REG_REG 3
#define OS2_REG_SENSOR_ID 9
#define OS2_REG_VALUE 7
#define OS2_REG_TS 8
#define OS2_REG_EVENT_MARK 10
#define OS2_REG_EVT_BUS 11
#define OS2_REG_EVT_ADDR 12
#define OS2_REG_EVT_REG 13
#define OS2_REG_CMD_PWM_SET_DUTY 14
#define OS2_REG_TMP 15

/* Mailbox backpressure policy (v1): reject new command when mailbox is full. */
#define OS2_MB_POLICY_REJECT_NEW 1
#define OS2_STATS_LOG_PERIOD_MS 5000U
#define OS2_RETRY_LIMIT 2U
#define OS2_RETRY_BACKOFF_MS 200U
#define OS2_DEGRADED_BACKOFF_MS 2000U
#define OS2_WDT_TIMEOUT_MS 6000U
#define OS2_WDT_DEGRADED_GRACE_MS 10000U
#define OS2_PWM_PERIOD_MS 1000U
#define OS2_PWM_CHANNEL 0U
#define OS2_PWM_ACTUATOR_ID 1U

/* Set >0 to inject a synthetic read failure every Nth sensor read. */
#ifndef OS2_FAULT_EVERY_N
#define OS2_FAULT_EVERY_N 0
#endif

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

typedef struct {
    uint32_t attempted;
    uint32_t pushed;
    uint32_t dropped_full;
    uint32_t processed;
} os2_mb_stats_t;

typedef struct {
    uint8_t id;
    uint8_t degraded;
    uint32_t read_count;
    uint32_t consecutive_errors;
    uint32_t backoff_until_ms;
    uint32_t degraded_since_ms;
} os2_sensor_runtime_t;

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
#define OS2_BOOT_MAGIC_GPR2 0xA5U

static uint16_t os2_boot_counter_next(void) {
    uint8_t magic = (uint8_t)(NRF_POWER->GPREGRET2 & 0xFFU);
    uint8_t counter = (uint8_t)(NRF_POWER->GPREGRET & 0xFFU);

    if (magic != OS2_BOOT_MAGIC_GPR2) {
        NRF_POWER->GPREGRET2 = OS2_BOOT_MAGIC_GPR2;
        counter = 0U;
    }

    counter = (uint8_t)(counter + 1U);
    if (counter == 0U) {
        counter = 1U;
    }
    NRF_POWER->GPREGRET = counter;
    return counter;
}

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
    size_t jz_to_pwm_patch;
    size_t jmp_over_pwm_patch;
    size_t jmp_to_loop_patch;
    size_t pwm_path_pc;

    /* r6 = idle sleep ms, r10 = published cmd type */
    os2_emit_u8(&pc, MB_OP_CONST_I32);
    os2_emit_u8(&pc, 6);
    os2_emit_i32(&pc, 20);

    os2_emit_u8(&pc, MB_OP_CONST_I32);
    os2_emit_u8(&pc, 10);
    os2_emit_i32(&pc, MB_CMD_NONE);

    os2_emit_u8(&pc, MB_OP_CONST_I32);
    os2_emit_u8(&pc, OS2_REG_CMD_PWM_SET_DUTY);
    os2_emit_i32(&pc, MB_CMD_PWM_SET_DUTY);

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

    /* if r0 == MB_CMD_PWM_SET_DUTY jump to pwm path */
    os2_emit_u8(&pc, MB_OP_SUB);
    os2_emit_u8(&pc, OS2_REG_TMP);
    os2_emit_u8(&pc, OS2_REG_CMD_TYPE);
    os2_emit_u8(&pc, OS2_REG_CMD_PWM_SET_DUTY);

    os2_emit_u8(&pc, MB_OP_JMP_IF_ZERO);
    os2_emit_u8(&pc, OS2_REG_TMP);
    jz_to_pwm_patch = pc;
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

    /* copy context to event registers */
    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, OS2_REG_EVT_BUS);
    os2_emit_u8(&pc, OS2_REG_BUS);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, OS2_REG_EVT_ADDR);
    os2_emit_u8(&pc, OS2_REG_ADDR);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, OS2_REG_EVT_REG);
    os2_emit_u8(&pc, OS2_REG_REG);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, 9);
    os2_emit_u8(&pc, 4);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, 10);
    os2_emit_u8(&pc, 0);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_JMP);
    jmp_over_pwm_patch = pc;
    os2_emit_i32(&pc, 0);

    pwm_path_pc = pc;
    /* PWM path:
     * r1=channel r2=permille r4=actuator_id, return code in r7
     */
    os2_emit_u8(&pc, MB_OP_CALL_BIF);
    os2_emit_u8(&pc, MB_BIF_PWM_SET_DUTY);
    os2_emit_u8(&pc, 2);
    os2_emit_u8(&pc, OS2_REG_BUS);
    os2_emit_u8(&pc, OS2_REG_ADDR);
    os2_emit_u8(&pc, OS2_REG_VALUE);

    os2_emit_u8(&pc, MB_OP_CALL_BIF);
    os2_emit_u8(&pc, MB_BIF_MONOTONIC_MS);
    os2_emit_u8(&pc, 0);
    os2_emit_u8(&pc, OS2_REG_TS);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, OS2_REG_EVT_BUS);
    os2_emit_u8(&pc, OS2_REG_BUS);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, OS2_REG_EVT_ADDR);
    os2_emit_u8(&pc, OS2_REG_ADDR);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_CONST_I32);
    os2_emit_u8(&pc, OS2_REG_EVT_REG);
    os2_emit_i32(&pc, 0);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, OS2_REG_SENSOR_ID);
    os2_emit_u8(&pc, OS2_REG_REG);
    os2_emit_u8(&pc, 0);

    os2_emit_u8(&pc, MB_OP_MOVE);
    os2_emit_u8(&pc, OS2_REG_EVENT_MARK);
    os2_emit_u8(&pc, OS2_REG_CMD_TYPE);
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

    os2_patch_rel_i32(jz_to_pwm_patch, pwm_path_pc);
    os2_patch_rel_i32(jmp_over_pwm_patch, pc);
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

static int os2_enqueue_sensor_cmd(mb_vm_t *vm, const os2_sensor_target_t *target, os2_mb_stats_t *stats) {
    mb_command_t cmd;
    int rc;

    stats->attempted++;

    cmd.type = MB_CMD_I2C_READ;
    cmd.a = target->bus;
    cmd.b = target->addr;
    cmd.c = target->reg;
    cmd.d = target->id;

#if OS2_MB_POLICY_REJECT_NEW
    if (vm->mailbox.count >= MB_MAILBOX_CAPACITY) {
        stats->dropped_full++;
        return MB_MAILBOX_FULL;
    }
#endif

    rc = mb_vm_mailbox_push(vm, cmd);
    if (rc == MB_OK) {
        stats->pushed++;
    } else if (rc == MB_MAILBOX_FULL) {
        stats->dropped_full++;
    }
    return rc;
}

static int os2_enqueue_pwm_cmd(mb_vm_t *vm, uint8_t channel, uint16_t duty_permille,
                               uint8_t actuator_id, os2_mb_stats_t *stats) {
    mb_command_t cmd;
    int rc;

    stats->attempted++;

    cmd.type = MB_CMD_PWM_SET_DUTY;
    cmd.a = channel;
    cmd.b = duty_permille;
    cmd.c = 0;
    cmd.d = actuator_id;

#if OS2_MB_POLICY_REJECT_NEW
    if (vm->mailbox.count >= MB_MAILBOX_CAPACITY) {
        stats->dropped_full++;
        return MB_MAILBOX_FULL;
    }
#endif

    rc = mb_vm_mailbox_push(vm, cmd);
    if (rc == MB_OK) {
        stats->pushed++;
    } else if (rc == MB_MAILBOX_FULL) {
        stats->dropped_full++;
    }
    return rc;
}

static os2_sensor_runtime_t *os2_find_runtime(os2_sensor_runtime_t *runtimes, size_t count, uint8_t sensor_id) {
    size_t i;
    for (i = 0; i < count; i++) {
        if (runtimes[i].id == sensor_id) {
            return &runtimes[i];
        }
    }
    return NULL;
}

static void os2_task_wdt_cb(int channel_id, void *user_data) {
    ARG_UNUSED(channel_id);
    ARG_UNUSED(user_data);
    LOG_ERR("task watchdog fired, rebooting");
    sys_reboot(SYS_REBOOT_COLD);
}

static int os2_task_wdt_start(void) {
    int rc = task_wdt_init(NULL);
    if (rc < 0) {
        return rc;
    }
    return task_wdt_add(OS2_WDT_TIMEOUT_MS, os2_task_wdt_cb, NULL);
}

static uint8_t os2_should_withhold_wdt_feed(const os2_sensor_runtime_t *runtimes, size_t count, uint32_t now_ms) {
    size_t i;
    for (i = 0; i < count; i++) {
        if (runtimes[i].degraded &&
            (now_ms - runtimes[i].degraded_since_ms) >= OS2_WDT_DEGRADED_GRACE_MS) {
            return 1U;
        }
    }
    return 0U;
}

int main(void) {
    mb_vm_t vm;
    int rc;
    os2_sensor_target_t targets[6];
    size_t target_count = 0;
    uint32_t last_ts = 0;
    uint32_t last_stats_ts = 0;
    size_t i;
    size_t program_size;
    os2_mb_stats_t mb_stats = {0};
    os2_sensor_runtime_t runtimes[6] = {0};
    int wdt_channel = -1;
    uint8_t wdt_feed_blocked = 0;
    uint32_t resetreas_raw = NRF_POWER->RESETREAS;
    uint16_t boot_counter = 0;
    uint32_t last_pwm_ts = 0;
    uint8_t pwm_step = 0;
    static const uint16_t pwm_pattern[] = {150U, 500U, 850U, 500U};

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
    LOG_INF("event schema v%d", OS2_EVENT_SCHEMA_VERSION);
    boot_counter = os2_boot_counter_next();
    LOG_INF("boot counter=%u resetreas=0x%08x", (unsigned)boot_counter, resetreas_raw);
    NRF_POWER->RESETREAS = resetreas_raw;

    wdt_channel = os2_task_wdt_start();
    if (wdt_channel < 0) {
        LOG_ERR("task_wdt init failed rc=%d", wdt_channel);
        return wdt_channel;
    }
    LOG_INF("task_wdt enabled timeout_ms=%u grace_ms=%u",
        OS2_WDT_TIMEOUT_MS, OS2_WDT_DEGRADED_GRACE_MS);

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
    for (i = 0; i < target_count; i++) {
        runtimes[i].id = targets[i].id;
    }

    program_size = os2_build_cyclic_program();
    mb_vm_init(&vm, demo_program, program_size);

    while (1) {
        uint32_t now_ms = k_uptime_get_32();
        if ((now_ms - last_pwm_ts) >= OS2_PWM_PERIOD_MS) {
            uint16_t duty = pwm_pattern[pwm_step % ARRAY_SIZE(pwm_pattern)];
            rc = os2_enqueue_pwm_cmd(&vm, OS2_PWM_CHANNEL, duty, OS2_PWM_ACTUATOR_ID, &mb_stats);
            if (rc != MB_OK) {
                LOG_WRN("mailbox full/drop actuator_id=%u rc=%d", OS2_PWM_ACTUATOR_ID, rc);
            } else {
                pwm_step++;
                last_pwm_ts = now_ms;
            }
        }
        for (i = 0; i < target_count; i++) {
            os2_sensor_runtime_t *rt = &runtimes[i];

            if (now_ms < rt->backoff_until_ms) {
                continue;
            }

            rc = os2_enqueue_sensor_cmd(&vm, &targets[i], &mb_stats);
            if (rc != MB_OK) {
                LOG_WRN("mailbox full/drop sensor_id=%u rc=%d", targets[i].id, rc);
                continue;
            }

            rc = mb_vm_run(&vm, 64);
            if (rc != MB_OK) {
                LOG_ERR("vm failed rc=%d last=%d pc=%u", rc, vm.last_error, (unsigned)vm.pc);
                return rc;
            }

            if (vm.regs[OS2_REG_EVENT_MARK] == MB_CMD_I2C_READ &&
                (uint32_t)vm.regs[OS2_REG_TS] != last_ts) {
                const char *name = "unknown";
                int32_t status = OS2_EVENT_STATUS_OK;
                int32_t event_value = vm.regs[OS2_REG_VALUE];
                uint8_t sensor_id = (uint8_t)vm.regs[OS2_REG_SENSOR_ID];
                uint8_t injected_fault = 0;
                os2_sensor_runtime_t *event_rt = NULL;
                size_t j;
                for (j = 0; j < target_count; j++) {
                    if (sensor_id == targets[j].id) {
                        name = targets[j].name;
                        break;
                    }
                }
                event_rt = os2_find_runtime(runtimes, target_count, sensor_id);

#if OS2_FAULT_EVERY_N > 0
                if (event_rt != NULL) {
                    event_rt->read_count++;
                    if ((event_rt->read_count % OS2_FAULT_EVERY_N) == 0U) {
                        event_value = -5;
                        injected_fault = 1;
                    }
                }
#else
                if (event_rt != NULL) {
                    event_rt->read_count++;
                }
#endif

                if (event_value < 0) {
                    status = OS2_EVENT_STATUS_IO_ERROR;
                    if (event_value == -22) {
                        status = OS2_EVENT_STATUS_BAD_ARGUMENT;
                    }
                    if (event_rt != NULL) {
                        event_rt->consecutive_errors++;
                        if (event_rt->consecutive_errors <= OS2_RETRY_LIMIT) {
                            status = OS2_EVENT_STATUS_RETRYING;
                            event_rt->backoff_until_ms = (uint32_t)vm.regs[OS2_REG_TS] + OS2_RETRY_BACKOFF_MS;
                        } else {
                            status = OS2_EVENT_STATUS_DEGRADED;
                            if (!event_rt->degraded) {
                                event_rt->degraded_since_ms = (uint32_t)vm.regs[OS2_REG_TS];
                            }
                            event_rt->degraded = 1;
                            event_rt->backoff_until_ms =
                                (uint32_t)vm.regs[OS2_REG_TS] + OS2_DEGRADED_BACKOFF_MS;
                        }
                    }
                } else if (event_rt != NULL) {
                    if (event_rt->degraded) {
                        status = OS2_EVENT_STATUS_RECOVERED;
                    }
                    event_rt->degraded = 0;
                    event_rt->consecutive_errors = 0;
                    event_rt->backoff_until_ms = 0;
                    event_rt->degraded_since_ms = 0;
                }

                LOG_INF("event sensor_id=%d name=%s bus=%d addr=0x%02x reg=0x%02x value=%d ts=%u status=%d inj=%u",
                    sensor_id,
                    name,
                    vm.regs[OS2_REG_EVT_BUS],
                    vm.regs[OS2_REG_EVT_ADDR],
                    vm.regs[OS2_REG_EVT_REG],
                    event_value,
                    (uint32_t)vm.regs[OS2_REG_TS],
                    status,
                    injected_fault);
                last_ts = (uint32_t)vm.regs[OS2_REG_TS];
                vm.regs[OS2_REG_EVENT_MARK] = MB_CMD_NONE;
                mb_stats.processed++;
            } else if (vm.regs[OS2_REG_EVENT_MARK] == MB_CMD_PWM_SET_DUTY &&
                       (uint32_t)vm.regs[OS2_REG_TS] != last_ts) {
                int32_t status = (vm.regs[OS2_REG_VALUE] < 0) ? OS2_EVENT_STATUS_IO_ERROR : OS2_EVENT_STATUS_OK;
                LOG_INF("actuator_event actuator_id=%d type=pwm_set_duty channel=%d duty_permille=%d value=%d ts=%u status=%d",
                    vm.regs[OS2_REG_SENSOR_ID],
                    vm.regs[OS2_REG_EVT_BUS],
                    vm.regs[OS2_REG_EVT_ADDR],
                    vm.regs[OS2_REG_VALUE],
                    (uint32_t)vm.regs[OS2_REG_TS],
                    status);
                last_ts = (uint32_t)vm.regs[OS2_REG_TS];
                vm.regs[OS2_REG_EVENT_MARK] = MB_CMD_NONE;
                mb_stats.processed++;
            }
        }
        if ((last_ts - last_stats_ts) >= OS2_STATS_LOG_PERIOD_MS) {
            LOG_INF("mb_stats attempted=%u pushed=%u dropped_full=%u processed=%u depth=%u/%u policy=reject_new",
                mb_stats.attempted,
                mb_stats.pushed,
                mb_stats.dropped_full,
                mb_stats.processed,
                (unsigned)vm.mailbox.count,
                (unsigned)MB_MAILBOX_CAPACITY);
            last_stats_ts = last_ts;
        }
        if (os2_should_withhold_wdt_feed(runtimes, target_count, k_uptime_get_32())) {
            if (!wdt_feed_blocked) {
                LOG_ERR("sensor degraded beyond grace; withholding task_wdt feed for recovery reboot");
                wdt_feed_blocked = 1;
            }
        } else {
            if (task_wdt_feed(wdt_channel) < 0) {
                LOG_ERR("task_wdt feed failed");
                return -1;
            }
            wdt_feed_blocked = 0;
        }
        k_msleep(200);
    }

    return 0;
}
