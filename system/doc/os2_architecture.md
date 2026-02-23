# OS/II Architecture

## Purpose

OS/II is a microcontroller operating environment for nRF52840-class hardware that combines:
- a restricted BEAM-like VM for orchestration, supervision, and policy logic
- native Zephyr/NCS-backed drivers for timing-sensitive peripheral execution

## Design Principles

1. Keep hard real-time and DMA-heavy work in native drivers.
2. Keep orchestration, retries, fault policies, and mode/state transitions in VM processes.
3. Use bounded resources and explicit contracts (command ABI + BIF surface).
4. Prefer deterministic runtime behavior over dynamic runtime features.

## Layered Architecture

```text
+------------------------------------------------------------------+
|                            Application Logic                      |
|                   (OS/II VM bytecode/modules)                    |
+-------------------------------+----------------------------------+
| VM Process Model              | VM Services                      |
| - mailbox-based processes     | - timers                         |
| - supervision policies        | - command validation             |
+-------------------------------+----------------------------------+
| VM Core                                                          |
| - opcode interpreter (register VM)                               |
| - BIF dispatch                                                     |
| - bounded mailbox + status/errors                                 |
+------------------------------------------------------------------+
| HAL/BIF Bridge                                                    |
| - gpio, pwm, i2c, time (current)                                 |
| - adc/spi/uart/qdec/audio (staged)                               |
+------------------------------------------------------------------+
| Native Driver Plane (Zephyr/NCS + nrfx fallback)                 |
| - ISR, DMA/EasyDMA, ring buffers                                 |
| - peripheral state machines                                       |
+------------------------------------------------------------------+
| Hardware: nRF52840 (GPIO, PWM, TWIM, SPIM, UARTE, SAADC, etc.)   |
+------------------------------------------------------------------+
```

## Runtime Partition

### VM Domain (Deterministic Control)

- Executes restricted opcode set.
- Holds control-state, mode transitions, fault/recovery logic.
- Consumes coarse events and issues coarse commands.

### Native Domain (Deterministic I/O + Throughput)

- Owns peripheral timing and DMA transfer lifecycle.
- Performs high-rate sampling/transfers and buffering.
- Reports summarized events/errors into VM mailbox.

## Core Execution Model

1. Native ISR/DMA completes a peripheral operation.
2. Native driver pushes an event payload into VM mailbox queue.
3. VM process reads event (`RECV_CMD` path) and applies policy.
4. VM emits command to HAL/BIF bridge.
5. Native driver executes command and returns status.

## Control and Data Flows

### Control Flow (Low bandwidth)

- VM -> BIF -> Native Driver
- Examples: set PWM duty, configure sampling, start/stop stream.

### Data/Event Flow (Bounded)

- Native Driver -> mailbox event -> VM
- Examples: sensor frame ready, threshold crossed, comms fault.

## Scheduling Strategy

1. Start with single VM scheduler loop.
2. Native concurrency handled by Zephyr threads/ISRs.
3. Introduce additional VM scheduling complexity only after peripheral stability.

## Memory Model

- Fixed register file per VM instance.
- Bounded mailbox capacity.
- Static code images.
- DMA buffers in RAM with explicit ownership.

## Fault Handling Model

1. Validate commands before enqueue and before dispatch.
2. Convert driver failures into typed status codes.
3. Route repeated faults into safe-state policy process.
4. Integrate watchdog only after M2 baseline is stable.

## Subsystem Roadmap Mapping (M0–M5)

1. M0: VM boot + static execution.
2. M1: GPIO/PWM/TWIM production path.
3. M2: SAADC/UARTE/timer-RTC.
4. M3: SPIM/QDEC/(PDM or I2S optional).
5. M4: resilience + soak.
6. M5: performance + contract freeze.

## Boundaries and Non-Goals (v1)

- No distributed Erlang.
- No hot code loading.
- No dynamic NIF/driver loading.
- No JIT.
- TWIS deferred until explicit requirement.

## Naming

- System name: `OS/II`
- Runtime module namespace suggestion: `os2_*`
- VM binary/profile suggestion: `os2_vm`
