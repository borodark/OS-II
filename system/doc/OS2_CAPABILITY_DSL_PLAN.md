# OS/II Capability DSL Plan (Board Abstraction Track)

## Goal

Define a capability-first DSL that lets us describe hardware resources (sensor buses, timers, PWM, ADC, BLE, power rails), compose control/data pipelines, and compile those plans into the restricted mini-BEAM runtime.

This turns board support from ad-hoc C wiring into a reusable contract:

1. Board advertises machine-readable capabilities.
2. DSL selects and binds resources.
3. Runtime executes with explicit timing, watchdog, and fallback policies.

## Current Baseline in This Repo

Already implemented and useful for this track:

1. Event schema lock (`event schema v2`) in `erts/example/mini_beam_esp32/zephyr_app/src/main.c`.
2. I2C signature probing and concrete sensor ID reads (APDS9960/HTS221/LPS22HB path).
3. Mailbox with bounded backpressure policy.
4. PWM actuator command path via VM mailbox dispatch.
5. Recovery behavior with watchdog hold/reboot evidence and boot counter checks.
6. Soak/perf scripts and regression gates (M5 assets under `system/doc/`).

## External Patterns We Should Reuse

Research snapshot date: 2026-02-25.

## 1) Zephyr Devicetree + Kconfig (resource declaration at build time)

- Zephyr already models buses, pins, and devices declaratively; overlays pick board variants.
- Fit for OS/II: keep low-level pinmux/device enable in devicetree, then export a runtime capability map to the VM layer.
- Source: <https://docs.zephyrproject.org/latest/build/dts/index.html>
- Source: <https://docs.zephyrproject.org/latest/build/kconfig/index.html>

## 2) Embedded HAL traits (portable driver surface)

- Rust `embedded-hal` demonstrates capability-shaped interfaces (`i2c`, `spi`, `pwm`, delays, etc.) decoupled from specific MCUs.
- Fit for OS/II: treat DSL ops like trait calls; backend adapter binds to Zephyr device instances.
- Source: <https://docs.rs/embedded-hal/latest/embedded_hal/>

## 3) W3C WoT Thing Description (semantic capability model)

- Standard JSON-LD for exposing properties/actions/events of devices.
- Fit for OS/II: northbound export of board/app capabilities for gateways/cloud tools.
- Source: <https://www.w3.org/WoT/>

## 4) EdgeX Device Profiles (declarative capability schemas)

- YAML-like declarative model of device resources/commands.
- Fit for OS/II: template for human-editable board profile and validation rules.
- Source: <https://docs.edgexfoundry.org/>

## 5) BEAM-on-MCU prior art (AtomVM) and BEAM-on-hardware ecosystems (Nerves/GRiSP)

- AtomVM proves practical BEAM subset/runtime on microcontrollers.
- Nerves/GRiSP show production BEAM hardware workflows and composable OTP design patterns.
- Fit for OS/II: keep VM tiny, move policy/orchestration into declarative contracts.
- Source: <https://atomvm.org/>
- Source: <https://www.atomvm.net/doc/main/release-notes.html>
- Source: <https://www.nerves-project.org/>
- Source: <https://hexdocs.pm/circuits_i2c/readme.html>
- Source: <https://www.grisp.org/>

## 6) Tock HIL/capsules model (capability virtualization pattern)

- Tock splits platform-specific HAL from reusable HIL + capsules and documents in-kernel virtualization for shared buses (SPI/I2C/UART/ADC, etc.).
- Fit for OS/II: good reference for capability virtualization and multi-client arbitration strategy.
- Source: <https://book.tockos.org/development/virtual>
- Source: <https://book.tockos.org/development/porting>

## Architecture Direction

## Capability layers

1. `board_caps` (detected/static):
`i2c`, `spi`, `pwm`, `adc`, `qdec`, `rtc`, `timers`, `i2s/pdm`, `ble`, `gpio`, `power_domains`.
2. `resource_bindings`:
which concrete peripheral instance/channel/pin is reserved for each logical role.
3. `flows`:
sensor -> transform -> decision -> actuator.
4. `policies`:
period/rate, deadline budget, queue bounds, watchdog policy, degrade policy.
5. `contracts`:
required vs optional capability; fallback behavior when missing.

## Proposed DSL shape (first pass)

```text
board nano33_ble_sense {
  caps: [i2c(2), spi(4), pwm(4), adc(8,200ksps), qdec, rtc(3), timers(5), ble, pdm, i2s]
}

resource env_i2c on i2c1 speed 400k
sensor temp_humid kind HTS221 bus env_i2c addr 0x5f id_reg 0x0f id_val 0xbc rate 25Hz
actuator motor_l kind pwm channel 0 freq 20000 via hbridge(in1=P0.13,in2=P1.08)
actuator motor_r kind pwm channel 1 freq 20000 via hbridge(in1=P0.14,in2=P1.09)

flow balance_loop {
  temp_humid -> filter(ema,alpha=0.2) -> control(pid,kp=30,ki=2,kd=1) -> [motor_l,motor_r]
  policy period 5ms deadline 4ms mailbox 16 watchdog 6000ms fallback motors_off
}
```

## Near-Term Execution Plan

## P0: Capability schema lock (2-3 days)

1. Define `os2_caps_v1` C struct + Erlang-term log format.
2. Emit one boot line with full capability payload.
3. Add parser/validator script.

Acceptance:

1. Boot log contains `os2_caps_v1 #{caps_v=>1,...}` with all required atoms.
2. Schema check script passes in CI/local.

## P1: Static profile file + binding validator (3-4 days)

1. Add profile file format (`profiles/nano33_ble_sense.os2`).
2. Validate logical resources against `os2_caps_v1`.
3. Reject invalid bindings before runtime loop starts.

Acceptance:

1. Invalid profile exits with explicit error.
2. Valid profile produces deterministic binding table in logs.

## P2: Flow compiler to mailbox bytecode (4-6 days)

1. Convert flow definitions to VM mailbox command sequences.
2. Support basic ops: `i2c_read`, `pwm_set_duty`, `sleep_until`, `emit_event`.
3. Add timing budget checks.

Acceptance:

1. Compiler emits repeatable bytecode for fixed input.
2. Runtime executes flow and logs per-step latency.

## P3: Policy engine (3-5 days)

1. Add declarative queue and watchdog policy binding.
2. Per-flow degrade/retry/fail-open behavior.

Acceptance:

1. Fault injection hits configured fallback path.
2. Recovery evidence includes policy IDs and transition reason.

## P4: Multi-board portability proof (4-7 days)

1. Run same DSL flow on 2+ boards with different profiles.
2. Only profile changes allowed; flow file unchanged.

Acceptance:

1. Identical flow compiles on both boards.
2. Capability mismatch is explicit and actionable.

## Research Questions (Thesis-friendly)

1. Can a capability DSL preserve deterministic timing under constrained VM execution?
2. What is the overhead of declarative policy vs hand-written C control loops?
3. Which capability abstractions are stable across nRF52/ESP32-class MCUs?
4. Can we prove graceful degradation properties from DSL contracts?

## Immediate Next Step

Implement `P0` now: lock `os2_caps_v1` in code + docs and emit capability payload as an Erlang map term at boot.
