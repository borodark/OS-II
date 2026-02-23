# OS/II Call for Contributions

OS/II is building a deterministic BEAM-inspired runtime for MCU orchestration. We are actively looking for contributors.

## What We Need Most

1. Peripheral Expansion
- Add and validate subsystem support for ADC, SPI, UART, timers/RTC, and audio interfaces.
- Keep the VM/native boundary explicit and bounded.

2. Reliability and Fault Handling
- Add fault-injection scenarios (I2C NACK storms, bus lockups, timeout paths).
- Improve restart/safe-state behavior and watchdog integration.

3. Measurement and Benchmarks
- Define reproducible latency/jitter/memory metrics.
- Compare VM-orchestrated flows to equivalent Zephyr-native task flows.

4. Tooling and Developer UX
- Better flash/monitor scripts and board profile handling.
- Structured debug output and machine-readable event logs.

## Contribution Principles
- Preserve deterministic behavior and bounded resource assumptions.
- Document decisions with rationale and measurable impact.
- Add acceptance tests for every new subsystem or ABI change.

## Immediate Starter Tasks

1. Add cyclic multi-sensor VM mailbox tests to host regression target.
2. Add SPI read/write BIF and one known device validation path.
3. Add typed event schema (`sensor_id`, `value`, `timestamp`, `status`) and log parser.

## Who Should Join
- Embedded systems engineers (Zephyr/nrfx, ARM Cortex-M)
- VM/runtime engineers
- Verification and benchmarking contributors
- Applied researchers in actor/concurrency systems on edge hardware

## Contact and Collaboration
Open an issue with:
- target board
- subsystem you want to work on
- proposed acceptance test
- expected metric impact (latency, memory, robustness, ergonomics)
