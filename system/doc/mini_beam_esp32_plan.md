# Mini BEAM Subset Plan for ESP32 (GPIO/PWM/I2C)

## Goal

Build a near-bare-metal BEAM-like runtime that orchestrates embedded peripherals
without full OTP/ERTS feature scope.

Primary hardware track: nRF52840 (Zephyr/NCS). ESP32 path remains optional.

## Explicit Non-Goals

- No distributed Erlang
- No hot code loading
- No dynamic NIF/driver loading
- No JIT
- No Wi-Fi/BLE in initial target

## Milestones

1. M0 Boot and Execute
- Bring up interpreter core and static bytecode image.
- Exit criteria: deterministic execution of demo program and clean halt.

2. M1 Peripheral BIF Surface
- Implement GPIO write/read, PWM set duty, and I2C read/write BIFs.
- Exit criteria: host sim and ESP32 hardware tests produce expected values.

3. M2 Process and Mailbox Basics
- Add lightweight process structure with mailbox and cooperative scheduling.
- Exit criteria: two processes exchange commands and coordinate I2C-to-PWM flow.

4. M3 Memory Safety
- Add bounded heaps and stop-the-world GC for small term subset.
- Exit criteria: long-run stability test without memory corruption or leaks.

5. M4 Robustness
- Add watchdog integration, peripheral fault handling, and restart strategy.
- Exit criteria: fault injection test recovers to known safe state.

6. M5 Performance Characterization
- Capture event-loop throughput, sensor polling period, and mailbox behavior
  under nominal and fault-injected load.
- Exit criteria: documented baseline report and repeatable measurement script.

## Runtime Boundaries

- Native layer owns hard real-time peripheral interaction.
- VM layer owns orchestration, state machine transitions, and retries.

## Migration Notes

- Contract v1 now uses 5-field mailbox commands `{type,a,b,c,d}`.
- `MB_OP_RECV_CMD` now decodes five destination registers.
- Added command/BIF surface for `GPIO_READ`, `I2C_WRITE`, and `PWM_CONFIG`.
- M2: added process model (`mb_process_t`) and cooperative scheduler
  (`mb_scheduler_t`).  New opcodes: `SEND (0x21)`, `SELF (0x22)`,
  `YIELD (0x23)`.  New error codes: `MB_SCHED_IDLE`, `MB_BAD_PID`,
  `MB_PROC_TABLE_FULL`.  `mb_vm_t` preserved as compat wrapper.
  Two-process I2C-to-PWM proof passes on host.
- M3: registers are now tagged `mb_term_t` (4-bit tags, 28-bit payload).
  Per-process semi-space heap with Cheney's copying GC.  New opcodes:
  `MAKE_TUPLE (0x50)`, `TUPLE_ELEM (0x51)`, `CONS (0x52)`, `HEAD (0x53)`,
  `TAIL (0x54)`.  New errors: `MB_HEAP_OOM`, `MB_BAD_TERM`, `MB_BAD_ARITY`.
  Long-run stability proven: 200K ticks, 100K messages, 2400+ GC cycles.

## Suggested RAM Budget (ESP32 initial)

- VM + code/data: 128 KB
- Process heaps/mailboxes: 96 KB
- I/O buffers: 32 KB
- Safety margin: 64 KB

Adjust after measurement on target board and toolchain.

## Current Status (2026-03-28)

- `M0`: done
- `M1`: done
- `M2`: done (host), pending Zephyr integration
- `M3`: done (host), pending Zephyr integration
- `M4`: done (hardware evidence captured in `system/doc/M4_RECOVERY_EVIDENCE.md`)
- `M5`: in progress (baseline tooling and first measurements started)
