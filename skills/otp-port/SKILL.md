---
name: otp-port
description: Study and plan porting of Erlang/OTP ERTS and BEAM runtime internals to constrained or non-standard targets (especially ESP32 and nRF52840 class environments). Use when mapping runtime subsystems, auditing platform assumptions, identifying blockers, or defining a staged porting strategy for ERTS or BEAM-subset runtimes.
---

# OTP Port

Use this skill to analyze ERTS/BEAM internals and produce implementation-ready plans for embedded targets.

## Workflow

1. Define target profile first.
- Capture CPU ISA, RAM/flash limits, MMU presence, OS model, networking availability, and dynamic loading availability.
- Choose scope explicitly: `full ERTS`, `subset runtime`, or `proof-of-execution`.

2. Load the source map.
- Read `references/erts-beam-map.md` for subsystem-to-file navigation.
- Read `references/beam-book-study-map.md` for BEAM design concepts relevant to runtime subset/port decisions.
- Prioritize `erts/configure.ac`, `erts/emulator/Makefile.in`, `erts/emulator/beam/erl_init.c`, `erts/emulator/sys/unix/sys.c`, and `erts/emulator/sys/common/*`.
- Use AtomVM as a comparative external source for BEAM-on-embedded tradeoffs: `https://atomvm.org/`.
- Use The BEAM Book as the conceptual source for VM/process/type/scheduling internals: `https://blog.stenmans.org/theBeamBook/`.

3. Perform target-specific gap analysis.
- For ESP32-class targets, read `references/esp32-porting-gap-analysis.md`.
- For nRF52840-class targets, read `references/nrf52840-register-machine-fit.md`.
- Classify each dependency as one of: `keep`, `replace`, `stub`, `disable`.
- Keep project decisions aligned with `system/doc/mini_beam_design_decisions.md`.

4. Define the minimum viable runtime profile.
- Disable unsupported or expensive features first (JIT, dynamic drivers/NIFs, heavy networking paths).
- Prefer interpreter (`emu`) flavor before any native code generation work.
- Propose concrete build settings and required source edits.

5. Produce staged execution plan.
- Stage A: bring up scheduler/threading + allocator + term representation.
- Stage B: boot minimal init path and execute small module.
- Stage C: re-enable selected subsystems incrementally.
- Attach measurable exit criteria for each stage.

## Output Contract

Return results in this order:
1. `Assumptions` (target facts and unknowns).
2. `Blockers` (hard incompatibilities with file references).
3. `Patch Surfaces` (where to edit in tree).
4. `Build Profile` (proposed knobs).
5. `Stage Plan` (ordered milestones with test gates).

## Constraints

- Treat upstream OTP as POSIX-first unless code proves otherwise.
- Prefer source-grounded claims with file references.
- Avoid broad rewrites until a minimal executable runtime exists.
