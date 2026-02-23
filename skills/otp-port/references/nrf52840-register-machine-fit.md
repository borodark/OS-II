# nRF52840 and BEAM Register-Machine Fit

This note explains how a BEAM-like register VM maps to nRF52840 hardware realities.

## What Matches Well

- BEAM is a virtual register machine with explicit operand fetch/decode.
- nRF52840 (Cortex-M4F @ 64 MHz) is good at tight interpreter loops in C.
- Small working sets and deterministic control loops (GPIO/PWM/I2C orchestration) are feasible.
- 256 KB RAM and 1 MB flash are enough for a subset VM with static bytecode and bounded heaps.

## What Does Not Match Automatically

- BEAM virtual registers are not hardware CPU registers.
- The VM register array lives in RAM, so each opcode handler still does memory loads/stores.
- Full OTP/ERTS assumptions (SMP schedulers, complex allocators, heavy I/O polling, dynamic loading) do not map to this MCU profile.
- Lack of MMU and tight RAM make full process heap semantics and GC policies expensive unless simplified.

## Practical Consequences

- Interpreter mode is the right baseline; JIT is out of scope.
- Keep opcode handlers simple and branch-predictable.
- Reserve native fast paths only for critical BIF/peripheral operations.
- Use VM for orchestration/state machines, not cycle-critical bit-banging loops.

## nRF52840-Oriented Guidance

1. Keep one scheduler/thread initially.
2. Use static module images and fixed opcode set.
3. Bound per-process memory and mailbox sizes.
4. Keep peripheral access behind a strict HAL (`gpio`, `pwm`, `i2c`, `time`).
5. Prefer Zephyr/NCS driver stack for reliability unless hard real-time constraints force deeper bare-metal paths.

## Decision Rule

For nRF52840, a BEAM-like subset is a good architectural fit for reliability and control logic.
A full BEAM/ERTS port is not a good fit without aggressive feature cuts.
