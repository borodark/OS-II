# OS/II Story Draft for GitHub

## Title
OS/II: A Deterministic BEAM-Inspired Runtime for nRF52840-Class Microcontrollers

## One-Paragraph Story
OS/II explores a practical question: can a restricted BEAM-style runtime provide clear orchestration semantics on resource-constrained microcontrollers without sacrificing deterministic hardware behavior? We implemented a mini register VM with bounded resources, mailbox-driven command flow, and a strict VM/native split. The VM handles control policy and sequencing; native drivers own timing-sensitive I/O. On Arduino Nano 33 BLE Sense hardware, the runtime now boots, enables sensor power rails, detects onboard sensors, and executes cyclic sensor reads through VM-mediated commands.

## Why This Is Interesting
- It keeps BEAM-like orchestration ideas while cutting full ERTS scope.
- It emphasizes deterministic execution boundaries over dynamic runtime features.
- It demonstrates real-board viability, not only host simulation.

## Current Technical Scope
- VM: interpreter-only, fixed register file, bounded mailbox.
- BIF/HAL: GPIO, PWM, I2C, monotonic time.
- Board path: Zephyr on nRF52840 (Nano 33 BLE / BLE Sense).
- Runtime behavior: sensor signature scan, bus selection, cyclic event loop.

## Explicit Non-Goals (Current Stage)
- No distributed Erlang.
- No hot code loading.
- No dynamic NIF loading.
- No JIT.

## Ergonomics and Observability
Developers can flash, monitor, and debug with short loops and explicit logs: sensor power enable, bus probe results, selected sensor targets, VM event emissions, and typed error paths. This reduces ambiguity during board bring-up and makes failures actionable.

## Suggested Repository Positioning
“A research-grade prototype for deterministic, BEAM-inspired embedded orchestration on MCU hardware.”

## Suggested Demo Output Snippet
```text
mini_beam_nrf52 start
vdd_env enabled on pin 22
i2c pull-up enabled on pin 0
mic_pwr enabled on pin 17
i2c bus1 probe APDS9960 addr=0x39 reg=0x92 val=0xab
i2c bus1 probe HTS221 addr=0x5f reg=0x0f val=0xbc
i2c bus1 probe LPS22HB addr=0x5c reg=0x0f val=0xb1
sensor detected: APDS9960 on bus1
vm done bus=1 addr=0x39 reg=0x92 i2c_value=171
```
