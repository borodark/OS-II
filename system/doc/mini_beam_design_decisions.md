# Mini BEAM Design Decisions

## Context

Target board family: nRF52840-class MCUs.
Goal: BEAM-like subset for GPIO/PWM/I2C orchestration.

## Locked Decisions

1. Runtime scope
- Use BEAM-like subset VM, not full OTP/ERTS.
- Keep interpreter-only execution (no JIT).

2. Platform strategy
- nRF52840 + Zephyr/NCS is the primary hardware/software path.
- ESP32 path remains optional/secondary.

3. Feature cuts
- No distributed Erlang.
- No hot code loading.
- No dynamic NIF/driver loading.
- Static bytecode/module images only.

4. Control architecture
- VM handles orchestration/state transitions/retry logic.
- Native HAL handles peripheral interaction and timing-sensitive operations.

5. Peripheral surface
- Required baseline: GPIO, PWM, I2C, monotonic time.
- Command ABI uses mailbox tuple `{type,a,b,c,d}`.

6. Interface stability
- Contract file is authoritative: `system/doc/mini_beam_esp32_contract_v1.md`.
- Any opcode/BIF/ABI change must update the contract and migration notes.

7. Memory policy
- Bounded resources first: fixed register file and mailbox capacity.
- Expand term/heap/GC only after peripheral path is stable on hardware.

## Review Rule

These decisions remain in force unless explicitly superseded by a new
versioned decision entry in this document.

## Decision Log

### 2026-02-23: Flashing Path for Arduino Nano 33 BLE

- Symptom observed: repeated `west flash` failures with `No device found` and
  `SAM-BA operation failed` while `/dev/ttyACM0` exists.
- Root cause class: Nano 33 BLE upload path is sensitive to bootloader timing
  and tool variant.
- Decision:
  - Prefer `nrf52840dk_nrf52840` for first hardware bring-up.
  - For Nano 33 BLE, use Arduino-packaged `bossac` binary as the runner tool
    (not distro `bossac` by default).
  - If USB bootloader flashing is unstable after deterministic retry steps,
    pivot to SWD programming/debug path (CMSIS-DAP/J-Link) instead of spending
    more iterations on SAM-BA timing.
- Rationale: this minimizes schedule risk and keeps OS/II runtime validation
  moving while preserving Nano compatibility as a secondary path.

### 2026-02-23: Sensor Event Schema Lock (M2)

- Decision: lock cyclic sensor event schema to
  `{sensor_id, value, ts, status}` with stable status enums.
- Implementation note: runtime may include extra transport context
  (`name,bus,addr,reg`) but core four fields are authoritative for parsers.
- Rationale: enables stable log parsing, metrics tooling, and fault analysis
  while M2 mailbox policy work proceeds.

### 2026-02-23: Mailbox Backpressure Policy Lock (M2)

- Decision: use `reject_new` when mailbox capacity is reached.
- Required runtime telemetry: `attempted`, `pushed`, `dropped_full`,
  `processed`, and queue depth.
- Rationale: deterministic drop behavior with explicit observability is simpler
  to validate than adaptive queue mutation in early M2.

### 2026-02-24: First-Pass Fault Recovery State Model (M4 bootstrap)

- Decision: extend event `status` with `RETRYING`, `DEGRADED`, and
  `RECOVERED` to make recovery transitions externally visible.
- Decision: add bounded backoff windows in the runtime:
  - retry backoff: `OS2_RETRY_BACKOFF_MS`
  - degraded backoff: `OS2_DEGRADED_BACKOFF_MS`
- Decision: add compile-time synthetic fault injection switch
  `OS2_FAULT_EVERY_N` for repeatable failure-path testing.
- Decision: add task watchdog recovery path; if degraded state persists beyond
  grace window, runtime withholds watchdog feed to force cold reboot.
- Rationale: this gives us low-cost, deterministic resilience telemetry before
  watchdog and restart orchestration are added.

### 2026-03-29: Process Model — Cooperative Round-Robin (M2)

- Decision: lightweight process structure (`mb_process_t`) with per-process
  register file, mailbox, program counter, and heap.  Maximum 8 processes.
- Decision: cooperative scheduler with 64-reduction time slices.
  `RECV_CMD` blocks (rewinds PC, sets WAITING) under the scheduler;
  `SLEEP_MS` records wake time and yields.
- Decision: three new opcodes (`SEND`, `SELF`, `YIELD`) for inter-process
  communication.  Send is non-blocking for the sender; receiver wakes
  immediately on message arrival.
- Decision: `mb_vm_t` preserved as a compatibility wrapper.
  New code should use `mb_scheduler_t` + `mb_process_t` directly.
- Rationale: the process model follows BEAM's per-process isolation
  (own registers, own mailbox, own heap) adapted for a fixed-slot table
  on a 256KB MCU.  Cooperative scheduling avoids preemption overhead and
  interrupt-safety complexity on Cortex-M4.

### 2026-03-29: Tagged Terms and Cheney's GC (M3)

- Decision: registers are `mb_term_t` (`uint32_t`) with 4-bit tags
  matching AtomVM's scheme: smallint (0xF), atom (0xB), PID (0x3),
  boxed/cons heap pointers (0x2/0x1).  28-bit signed integer payload.
- Decision: per-process semi-space heap (128 words per space, 512 bytes
  each).  Cheney's copying GC with no recursion — BFS scan only.
- Decision: external mailbox ABI (`mb_command_t`) stays raw `int32_t`.
  The tagging boundary is at `RECV_CMD` (tags incoming fields) and
  `SEND` (untags outgoing fields).
- Rationale: 4-bit tags give smallint, atom, PID, tuple, and cons
  as immediates or heap pointers with no boxing overhead for the common
  case (integers).  Cheney's GC is the simplest copying collector and
  uses no stack recursion — critical for the 8KB Zephyr main stack.
  Per-process collection means only one process pauses at a time,
  exactly as in BEAM.

### 2026-03-29: Flow File Format — Erlang Terms (P2)

- Decision: flow definitions use Erlang term syntax (`.flow` files
  parsed by `file:consult/1`).  The flow compiler is an escript that
  reads the term, validates it, and emits a C header with bytecode
  arrays.
- Decision: the flow compiler generates separate bytecode programs for
  each sensor process and one actuator process.  The Zephyr runtime
  spawns them via the scheduler at boot.
- Decision: `poll_ms => 0` emits `YIELD` instead of `SLEEP_MS`,
  enabling tight-loop stress testing.
- Alternatives considered:
  - Custom key=value format (P1 profile style): rejected because it
    requires inventing a parser and cannot express nested structures
    (lists of sensors, tuple references for actuators).
  - INI-style sections: rejected for the same reasons, plus ambiguity
    in cross-references between sections.
  - JSON: rejected because it adds a parser dependency and does not
    align with the Erlang ecosystem that OS/II is part of.
- Rationale: the runtime already emits Erlang terms (`os2_caps_v1`
  boot schema).  Using the same format for flow input means one
  serialization through the entire stack.  Erlang's `file:consult`
  provides free parsing and validation.  The format is natural for
  another Claude or any Erlang developer to produce from a verbal
  specification.

### 2026-03-29: I2C Probe Timeout via Dedicated Thread

- Decision: I2C signature probes run in a dedicated Zephyr thread
  with a 500ms semaphore timeout.  If the probe thread does not
  complete, it is aborted and the address is skipped.
- Root cause: the nRF52 TWIM driver blocks indefinitely when a sensor
  holds SDA low (observed with APDS9960 after unclean shutdown).
  The Zephyr I2C API has no per-call timeout parameter.
- Alternatives considered:
  - `i2c_recover_bus()`: tested, did not release the stuck bus.
  - VDD_ENV power cycle: tested, sensor retained state across cycle.
  - Skip known-bad addresses: works but is board-specific and fragile.
- Rationale: a thread with a semaphore timeout is the only reliable
  way to bound the probe duration without modifying the Zephyr I2C
  driver.  The 1KB stack cost is acceptable (0.4% of RAM).

### 2026-03-29: Stress Test Boundary — I2C Bus, Not Scheduler (M5)

- Decision: accept the I2C bus at 400kHz as the throughput ceiling
  for sensor workloads.  Do not pursue pure-compute benchmarks that
  remove I2C from the loop.
- Measurement: 4 sensor processes in tight loops (YIELD, no sleep)
  SENDing to 1 actuator process:
  - 995 I2C events/s (bus-saturated)
  - 4,023 scheduler ticks/s
  - 0/32 mailbox peak depth (actuator always keeps up)
  - 0 errors over 25+ seconds
  - CPU at 100% utilization, zero idle ticks
- Scheduler idle capacity: 120,000 ticks/s (measured in baseline test
  with processes sleeping).  Under full I2C load, only 3% of scheduler
  capacity is consumed.
- Rationale: every real workload on this board involves peripheral I/O.
  The runtime's job is to be invisible behind the I/O — and at <3% CPU
  overhead under full load, it is.  A synthetic benchmark without I/O
  measures something no one will deploy.
