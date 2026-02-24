# OS/II Runtime Research


## why

```
The Nordic Semiconductor nRF52840 SoC, featuring a 64 MHz ARM Cortex-M4 with a Floating Point Unit (FPU), 1MB Flash, and 256KB RAM, provides computing power that is significantly higher than typical mid-90s consumer PCs in terms of raw CPU efficiency, despite having far less memory and storage. [1, 2, 3, 4, 5].
It is best compared to a high-end 1994–1995 desktop PC, such as a Compaq Presario running an Intel DX4-100 or an early 60MHz/66MHz Pentium processor. [6, 7, 8]  almost without RAM, 256KB must be plenty.

Performance Comparison Points

• CPU Speed (64 MHz Cortex-M4 vs. 66 MHz Pentium/100 MHz 486): The Cortex-M4 architecture is vastly more efficient per clock cycle than 90s processors. While the clock speed (64 MHz) matches a mid-90s Pentium, the CoreMark benchmark score (roughly 215 CoreMark) means the nRF52840 handles math and logic operations more like a slightly faster Pentium.

• Floating Point Unit (FPU): The nRF52840 includes a hardware FPU. In the early 90s, this was a premium feature (like a 486DX vs 486SX) or required a dedicated math coprocessor, making the nRF52840's floating-point performance superior to standard 1990–1993 486 machines.

• RAM (256 KB vs. 4 MB–8 MB): This is the main difference. A 90s PC needed massive RAM to load Windows 3.1/95. The nRF52840 is an embedded system, meaning 256KB of RAM is generous for its tasks, but it is vastly lower than the megabytes used in 1995. So its almost without RAM

• Compute Density: The nRF52840 fits a processor, radio (Bluetooth 5), and memory into a single chip smaller than a fingernail, whereas a 90s computer required a motherboard, multiple cards, and a large power supply to achieve similar raw CPU throughput. [1, 9, 10, 11, 12]

Summary: For raw, specialized, and efficient calculation (especially floating-point), the nRF52840 is roughly comparable to a 1994 Compaq Deskpro with a 66MHz Intel Pentium. [2, 11, 13] almost without RAM


AI responses may include mistakes.

[1] https://www.nordicsemi.com/Products/nRF52840
[2] http://files.pine64.org/doc/datasheet/pinetime/nRF52840%20product%20brief.pdf
[3] https://www.ultralibrarian.com/2026/1/9/nrf52840-datasheet-explained/
[4] https://www.nordicsemi.com/Nordic-news/2018/11/nRF52840-is-one-of-the-first-devices-to-support-Bluetooth-LE-with-Amazon-FreeRTOS
[5] https://www.digikey.com/en/product-highlight/n/nordic-semi/nrf52840-multi-protocol-soc
[6] https://www.vogons.org/viewtopic.php?t=54682
[7] https://www.vogons.org/viewtopic.php?t=75199
[8] https://en.wikipedia.org/wiki/Pentium_(original)
[9] https://www.reddit.com/r/digitalfoundry/comments/1m88nav/how_much_more_powerful_are_modern_pcs_than_early/
[10] https://forum.seeedstudio.com/t/nrf52840-product-specification-v1-7-revision-history-november-2021/273218
[11] https://www.ultralibrarian.com/2026/1/9/nrf52840-datasheet-explained/
[12] https://www.ic-components.com/blog/Comparing-NRF5340-and-NRF52840-Bluetooth-LE,and-NFC.jsp
[13] https://www.mouser.com/datasheet/2/297/nrf52840_soc_v3_0-2942478.pdf
```


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

## Roadmap (M0–M5)

- `M0` VM boot + static program execution (done)
- `M1` GPIO/PWM/I2C orchestration on hardware (done baseline)
- `M2` sensor event loop + mailbox policy expansion (in progress)
- `M3` memory model hardening + long-run stability tests
- `M4` resilience features: watchdog, fault recovery, soak runs (done)
- `M5` performance characterization + contract freeze (in progress)

M5 starter tooling:
- `erts/example/mini_beam_esp32/zephyr_app/analyze_event_perf.sh`
- `system/doc/M5_PERF_BASELINE.md`

## Repository Split Note

This is a standalone OS/II project repository.  
The original upstream OTP clone was moved to:
- `../otp_github`

## License

This repository is licensed under Apache-2.0:
- `LICENSE`
