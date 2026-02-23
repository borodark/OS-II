# OS/II Runtime Research

Deterministic, BEAM-inspired embedded runtime research for microcontrollers.

This repo contains the active OS/II prototype:
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
- Zephyr workspace (any path)
- Arduino `bossac` installed at `~/.arduino15/.../bossac`
- board connected on `/dev/ttyACM0`

Run build + flash + monitor:
```bash
export ZEPHYR_WS="$HOME/zephyrproject"
env CCACHE_DISABLE=1 XDG_CACHE_HOME=/tmp/zephyr-cache \
  ./erts/example/mini_beam_esp32/zephyr_app/reflash_nano33_sense.sh --monitor
```

## Bootstrap From Fresh Clone

Create a Zephyr workspace + venv and install west dependencies:
```bash
./erts/example/mini_beam_esp32/zephyr_app/bootstrap_zephyr_workspace.sh "$HOME/zephyrproject"
```

Then build/flash:
```bash
source "$HOME/zephyrproject/.venv/bin/activate"
export ZEPHYR_WS="$HOME/zephyrproject"
export BOSSAC="$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac"
./erts/example/mini_beam_esp32/zephyr_app/reflash_nano33_sense.sh --monitor
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

## Repository Split Note

This is a standalone OS/II project repository.  
The original upstream OTP clone was moved to:
- `../otp_github`

## License

This repository is licensed under Apache-2.0:
- `LICENSE`
