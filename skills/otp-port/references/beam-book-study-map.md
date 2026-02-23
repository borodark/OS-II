# BEAM Book Study Map for OTP Port Skill

Source: https://blog.stenmans.org/theBeamBook/

This map defines what to read first when designing or porting a BEAM-like runtime.

## Priority Reading Order (for embedded/runtime porting)

1. Processes
- Focus: process memory layout, mailbox behavior, and process control block concepts.
- Why: defines the minimum process abstraction needed even for a subset runtime.

2. Type System and Tags
- Focus: term tagging model and boxed/immediate representation.
- Why: determines data model choices and memory pressure characteristics.

3. BEAM VM chapter
- Focus: virtual register model, interpreter behavior, switching, and memory management interactions.
- Why: anchors dispatch loop and register-file design.

4. Generic Instructions and Calls/Linking/Code Loading
- Focus: instruction categories and call model.
- Why: helps choose a constrained opcode subset and call boundaries.

5. Scheduling
- Focus: reductions, process states, scheduler loop design.
- Why: informs single-scheduler-first strategy on constrained MCUs.

6. Memory Subsystem + Garbage Collection
- Focus: allocator tradeoffs and GC implications.
- Why: guides bounded heap/mailbox choices for MCU targets.

7. IO, Ports, Networking + NIF/BIF chapter
- Focus: boundary between VM and external world.
- Why: supports the design decision that hardware access stays in native HAL/BIFs.

## Embedded Interpretation Rules

- Treat BEAM as a virtual machine design reference, not a requirement to replicate full OTP/ERTS behavior.
- Preserve semantics that matter for reliability (message passing, isolation, fault boundaries), simplify everything else.
- Prefer static modules and deterministic runtime shapes before any dynamic facilities.

## Specific Relevance to nRF52840 Subset Runtime

- Virtual register machine maps well to an interpreter loop in C on Cortex-M4F, but virtual registers are RAM-backed, not CPU-register-resident.
- ISR/DMA-heavy peripherals (SAADC, I2S, PDM, SPI/TWI/UARTE EasyDMA paths) should remain native; VM consumes events/chunks.
- Use BEAM ideas for orchestration and supervision, not for sample-by-sample high-rate data plane execution.

## Skill Usage Requirement

When this skill is used for architecture decisions, include at least one explicit reference to this study map and one explicit reference to `references/nrf52840-register-machine-fit.md` (for nRF targets) or `references/esp32-porting-gap-analysis.md` (for ESP32 targets).
