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
