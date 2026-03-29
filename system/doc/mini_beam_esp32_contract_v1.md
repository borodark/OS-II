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
  - Compat mode (no scheduler): non-blocking, returns `MB_CMD_NONE` on empty.
  - Scheduler mode: blocks (rewinds PC, sets WAITING) until message arrives.
- `MB_OP_SEND (0x21)` with register operands: `r_pid,r_type,r_a,r_b,r_c,r_d`
  - Sends command to target process's mailbox. Result code in `regs[r_pid]`.
  - Wakes target if WAITING. Returns `MB_BAD_PID` for invalid target.
- `MB_OP_SELF (0x22)` with operand: `r_dst`
  - Writes process PID to `regs[r_dst]`.
- `MB_OP_YIELD (0x23)` (no operands)
  - Exhausts reduction budget, returning control to scheduler.
- `MB_OP_JMP (0x30)`
- `MB_OP_JMP_IF_ZERO (0x31)`
- `MB_OP_SLEEP_MS (0x40)`
- `MB_OP_MAKE_TUPLE (0x50)` with operands: `r_dst, arity, r0, r1, ...`
  - Heap-allocates a tuple. Triggers GC on allocation failure.
- `MB_OP_TUPLE_ELEM (0x51)` with operands: `r_dst, r_tuple, index`
  - Extracts element at index from a boxed tuple.
- `MB_OP_CONS (0x52)` with operands: `r_dst, r_head, r_tail`
  - Heap-allocates a cons cell. Triggers GC on allocation failure.
- `MB_OP_HEAD (0x53)` with operands: `r_dst, r_cons`
  - Extracts head (car) of a cons cell.
- `MB_OP_TAIL (0x54)` with operands: `r_dst, r_cons`
  - Extracts tail (cdr) of a cons cell.
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
- `MB_SCHED_IDLE = 10`
- `MB_BAD_PID = 11`
- `MB_PROC_TABLE_FULL = 12`
- `MB_HEAP_OOM = 13`
- `MB_BAD_TERM = 14`
- `MB_BAD_ARITY = 15`

Hard decode/runtime errors abort `mb_vm_run()`. Mailbox empty on `MB_OP_RECV_CMD` is non-fatal.

## 9. Process Model (M2)

- Process table: `MB_MAX_PROCESSES = 8` slots.
- PID: `uint8_t`, 1-based index (0 = `MB_PID_NONE`).
- States: `FREE`, `READY`, `WAITING`, `SLEEPING`, `HALTED`.
- Each process owns: register file, program counter, mailbox.
- Scheduler: cooperative round-robin, `MB_REDUCTIONS = 64` steps per tick.
- `SLEEP_MS` in scheduler mode is non-blocking (records wake time).
- Inter-process communication via `SEND` opcode or `mb_sched_send()` from native code.

## 10. Term Representation and Heap (M3)

- Register type: `mb_term_t` (`uint32_t`) with 4-bit tags.
- Tag scheme (AtomVM-inspired):
  - `0xF` = small integer (28-bit signed, range -134M to +134M)
  - `0xB` = atom (well-known: nil=0, true=1, false=2)
  - `0x3` = PID
  - `0x2` = boxed heap pointer (tuple)
  - `0x1` = cons heap pointer (list cell)
- Per-process heap: two semi-spaces of `MB_HEAP_WORDS` words (default 128 = 512 bytes each).
- Allocation: bump pointer in active (from) space.
- GC: Cheney's copying collector, triggered on allocation failure.
  - Root set: all 16 registers.
  - No recursion (BFS scan) — safe for Cortex-M4 small stacks.
  - Per-process, so only one process pauses at a time (BEAM model).
- Tuple layout on heap: `[header_word, elem_0, ..., elem_{arity-1}]`.
  - Max arity: `MB_MAX_TUPLE_ARITY = 16`.
- Cons cell layout: `[head, tail]` (2 words, no header).
- External mailbox ABI (`mb_command_t`) stays raw `int32_t`.
  Tagging boundary is at `RECV_CMD` (tags incoming) and `SEND` (untags outgoing).
- Stability proven: 200K scheduler ticks, 100K messages, 2400+ GC cycles, zero errors.

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
