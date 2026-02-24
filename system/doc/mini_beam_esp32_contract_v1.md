# Mini BEAM ESP32 Contract v1

This document freezes the current VM interface for short-term development.

## 1. Register Model

- Register file size: `MB_REG_COUNT = 16`.
- Register type: signed 32-bit integer.
- Uninitialized registers are zero after `mb_vm_init`.

## 2. Opcode Contract (v1)

- `MB_OP_NOP (0x00)`
- `MB_OP_CONST_I32 (0x01)`
- `MB_OP_MOVE (0x02)`
- `MB_OP_ADD (0x03)`
- `MB_OP_SUB (0x04)`
- `MB_OP_CALL_BIF (0x10)`
- `MB_OP_RECV_CMD (0x20)` with register operands: `r_type,r_a,r_b,r_c,r_d`
- `MB_OP_JMP (0x30)`
- `MB_OP_JMP_IF_ZERO (0x31)`
- `MB_OP_SLEEP_MS (0x40)`
- `MB_OP_HALT (0xFF)`

Byte encoding is little-endian for all 32-bit immediates.

## 3. Mailbox Command ABI (v1)

Command shape: `{type, a, b, c, d}` using `int32_t` fields.

- `MB_CMD_NONE = 0`
- `MB_CMD_GPIO_WRITE = 1` with args: `a=pin`, `b=level`
- `MB_CMD_PWM_SET_DUTY = 2` with args: `a=channel`, `b=permille(0..1000)`
- `MB_CMD_I2C_READ = 3` with args: `a=bus`, `b=addr`, `c=reg`
- `MB_CMD_GPIO_READ = 4` with args: `a=pin`
- `MB_CMD_I2C_WRITE = 5` with args: `a=bus`, `b=addr`, `c=reg`, `d=value`
- `MB_CMD_PWM_CONFIG = 6` with args: `a=channel`, `b=frequency_hz`

Validation is enforced on mailbox push and command decode.

## 4. BIF Contract (v1)

- `MB_BIF_GPIO_WRITE = 1` args: `(pin, level)` result: status code in dst register.
- `MB_BIF_PWM_SET_DUTY = 2` args: `(channel, permille)` result: status code.
- `MB_BIF_I2C_READ_REG = 3` args: `(bus, addr, reg)` result: byte value (0..255) or negative error code.
- `MB_BIF_MONOTONIC_MS = 4` args: `()` result: monotonic time in ms (truncated to int32).
- `MB_BIF_GPIO_READ = 5` args: `(pin)` result: level (0/1) or negative error code.
- `MB_BIF_I2C_WRITE_REG = 6` args: `(bus, addr, reg, value)` result: status code.
- `MB_BIF_PWM_CONFIG = 7` args: `(channel, frequency_hz)` result: status code.

## 5. Status/Error Codes

- `MB_OK = 0`
- `MB_EOF = 1`
- `MB_BAD_REG = 2`
- `MB_BAD_OPCODE = 3`
- `MB_BAD_BIF = 4`
- `MB_BAD_ARGC = 5`
- `MB_MAILBOX_EMPTY = 6`
- `MB_INVALID_COMMAND = 7`
- `MB_BAD_ARGUMENT = 8`
- `MB_MAILBOX_FULL = 9`

Hard decode/runtime errors abort `mb_vm_run()`. Mailbox empty on `MB_OP_RECV_CMD` is non-fatal.

## 6. Stability Rule

Any opcode/BIF/ABI change must:
1. Update this contract file.
2. Keep host demos compiling.
3. Include migration note in `system/doc/mini_beam_esp32_plan.md`.

## 7. Sensor Event Schema (v1)

For cyclic sensor orchestration, OS/II emits typed events in log form with
locked fields and ordering:

`sensor_id, value, ts, status`

Extended emitted record currently includes transport context:
`sensor_id, name, bus, addr, reg, value, ts, status`

### Field Definitions

- `sensor_id` (int32): stable sensor target identifier from discovery table.
- `value` (int32): sensor read value (0..255) or negative error code.
- `ts` (uint32 in log): monotonic timestamp in milliseconds.
- `status` (int32 enum):
  - `0` = `OS2_EVENT_STATUS_OK`
  - `1` = `OS2_EVENT_STATUS_IO_ERROR`
  - `2` = `OS2_EVENT_STATUS_BAD_ARGUMENT`
  - `3` = `OS2_EVENT_STATUS_INTERNAL_ERROR`
  - `4` = `OS2_EVENT_STATUS_RETRYING`
  - `5` = `OS2_EVENT_STATUS_DEGRADED`
  - `6` = `OS2_EVENT_STATUS_RECOVERED`

### Mapping Rule (v1)

- If `value >= 0` then `status = OS2_EVENT_STATUS_OK`.
- If `value < 0` then `status` is mapped from error class (currently I/O or
  argument validation paths).
- First-pass resilience behavior:
  - first `OS2_RETRY_LIMIT` consecutive failures emit `RETRYING`
  - failures beyond retry limit emit `DEGRADED`
  - first subsequent successful read after degraded state emits `RECOVERED`
  - degraded/retry backoff windows are controlled by
    `OS2_RETRY_BACKOFF_MS` and `OS2_DEGRADED_BACKOFF_MS`
  - if degraded persists past watchdog grace window, runtime triggers cold
    reboot recovery through task watchdog timeout

### Fault Injection Switch

- Build-time macro: `OS2_FAULT_EVERY_N` (default `0`, disabled).
- When set to `N > 0`, every Nth read per sensor runtime is forced to an I/O
  error path for resilience testing.

## 8. Mailbox Backpressure Policy (v1)

Policy: `reject_new` when mailbox is full.

- Queue capacity remains `MB_MAILBOX_CAPACITY`.
- On enqueue attempt while full, command is dropped and counted.
- Runtime counters are emitted periodically:
  - `attempted`
  - `pushed`
  - `dropped_full`
  - `processed`
  - queue depth (`depth/capacity`)

This policy is deterministic and side-effect free on existing queued commands.
