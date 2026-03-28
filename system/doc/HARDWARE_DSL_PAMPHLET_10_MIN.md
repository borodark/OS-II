# OS/II Hardware DSL in 10 Minutes

## 1) What problem this solves

Without DSLs, every board/app change means C rewiring.
With DSLs, board limits and app intent become data that can be validated before runtime.

Result:
- safer deploys,
- faster iteration,
- portable behavior across boards.

## 2) The 3 DSL layers

## Layer A: Capability DSL (`what exists`)

Published by firmware at boot:

`os2_caps_v1 #{caps_v=>1,board=>...,i2c=>...,pwm=>...,adc=>#{...},...}`

This is the machine-readable contract of hardware capabilities and limits.

## Layer B: Profile DSL (`how we bind`)

Human-edited file mapping logical roles to real resources:

`erts/example/mini_beam_esp32/zephyr_app/profiles/nano33_ble_sense.os2`

Examples:
- `bind.sensor_bus=1`
- `bind.pwm_channel=0`
- `require.pwm_min=4`

## Layer C: Flow DSL (`what should happen`)

Declares behavior over time:
- read sensor,
- transform/filter,
- decide,
- actuate,
- enforce timing/fallback policy.

This compiles to mailbox commands for the restricted BEAM runtime.

## 3) End-to-end mental model

1. Firmware boots and emits `os2_caps_v1`.
2. Validator checks profile requirements against capabilities.
3. Valid profile yields a deterministic binding table.
4. Flow uses those bindings to run control logic.
5. Runtime emits evidence logs for events and status.

## 4) 10-minute hands-on (current repo)

From `zephyr_app` directory:

```bash
cd /home/io/projects/learn_erl/otp/erts/example/mini_beam_esp32/zephyr_app
```

Validate boot capability term in a log:

```bash
./validate_caps_term.sh logs/nano33.log
```

Validate profile/bindings against that capability term:

```bash
./validate_profile_bindings.sh \
  --profile profiles/nano33_ble_sense.os2 \
  --log logs/nano33.log
```

Expected success:
- `PASS: os2_caps_v1 term found and validated`
- `PASS: profile ... is compatible ...`
- `binding_table: ...`

## 5) Why this is strong for research and products

- Clear separation of concerns:
  - hardware facts,
  - binding choices,
  - behavior policy.
- Reproducibility:
  same profile + same flow + same caps contract => comparable runs.
- Auditability:
  failures are explicit (missing capability, invalid binding, policy mismatch).

## 6) Next steps

1. Add first flow DSL file for one closed loop (`sensor -> filter -> pwm`).
2. Compile flow to mailbox bytecode.
3. Log flow ID + policy ID in runtime events for full traceability.
