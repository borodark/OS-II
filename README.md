# OS/II on Erlang/OTP Source Tree

Deterministic, BEAM-inspired embedded runtime research on top of the Erlang/OTP source tree.

This repo contains upstream OTP sources plus an active research prototype:
- `OS/II` mini VM runtime for MCU orchestration
- Zephyr hardware path for `nRF52840` boards
- validated bring-up on Arduino Nano 33 BLE Sense sensors

## Project Focus

`OS/II` tests a constrained model:
- VM (register interpreter) handles orchestration/state policy
- Native drivers handle timing-sensitive peripheral operations
- Bounded mailbox command ABI connects both sides

Current scope is intentionally restricted:
- no distributed Erlang
- no hot code loading
- no dynamic NIF loading
- no JIT

## Hardware-Proven Status

On Nano 33 BLE Sense we have verified:
- USB flash/boot/console loop
- sensor rail and I2C pull-up enable
- onboard sensor detection on internal bus
- VM-mediated cyclic sensor reads

Example runtime signals:
```text
i2c bus1 probe APDS9960 addr=0x39 reg=0x92 val=0xab
i2c bus1 probe HTS221 addr=0x5f reg=0x0f val=0xbc
i2c bus1 probe LPS22HB addr=0x5c reg=0x0f val=0xb1
sensor detected: APDS9960 on bus1
```

## Quickstart (Sense)

Requirements:
- Zephyr workspace at `/home/io/zephyrproject`
- Arduino `bossac` installed at `~/.arduino15/.../bossac`
- board connected on `/dev/ttyACM0`

Run build + flash + monitor:
```bash
env CCACHE_DISABLE=1 XDG_CACHE_HOME=/tmp/zephyr-cache \
  /home/io/projects/learn_erl/otp/erts/example/mini_beam_esp32/zephyr_app/reflash_nano33_sense.sh --monitor
```

## Key Paths

- mini VM + HAL:
  - `erts/example/mini_beam_esp32/include`
  - `erts/example/mini_beam_esp32/src`
- Zephyr app:
  - `erts/example/mini_beam_esp32/zephyr_app`
- architecture and plan docs:
  - `system/doc/os2_architecture.md`
  - `system/doc/os2_architecture_diagram.md`
  - `system/doc/os2_architecture_ascii.md`
  - `system/doc/mini_beam_design_decisions.md`
  - `system/doc/mini_beam_esp32_plan.md`

## Architecture

- Rendered SVG: `system/doc/os2_architecture.svg`
- Mermaid diagram: `system/doc/os2_architecture_diagram.md`
- ASCII fallback: `system/doc/os2_architecture_ascii.md`
- Detailed write-up: `system/doc/os2_architecture.md`

## Publish Drafts

- story draft: `system/doc/README_GITHUB_STORY.md`
- contributor call: `system/doc/CALL_FOR_CONTRIBUTIONS.md`
- MSc/PhD framing: `system/doc/RESEARCH_PLAN_MSC_PHD.md`

## Upstream OTP Context

This repository still includes the full Erlang/OTP source tree. For upstream OTP build and contribution workflow, see:
- `HOWTO/INSTALL.md`
- `CONTRIBUTING.md`
- `SECURITY.md`

## License

Erlang/OTP and this work remain under Apache-2.0 terms in this repository:
- `LICENSE.txt`
