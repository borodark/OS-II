# OS/II Nano 33 BLE Flash Research (2026-02-23)

## Why We Hit the Loop

- Zephyr documentation for `arduino_nano_33_ble` notes that uploads use
  BOSSA and explicitly call out Arduino-specific `bossac` packaging.
- Arduino Nano 33 BLE behavior can require entering bootloader mode manually
  (double reset) and can re-enumerate USB serial interfaces between runtime and
  bootloader states.

## Decision

1. Keep `nrf52840dk_nrf52840` as primary bring-up board.
2. Keep Nano 33 BLE support, but with strict upload procedure:
- Arduino `bossac` binary
- explicit bootloader timing
- explicit port probing (`/dev/ttyACM0`, `/dev/ttyACM1`)
3. If this still fails repeatedly, use SWD flashing/debug and stop spending
   schedule on USB bootloader variability.

## Practical Branching Strategy

1. USB branch (attempt up to 3 deterministic cycles):
- Double-tap reset -> LED pulse
- Flash immediately with Arduino `bossac`
- Capture `dmesg -w` + `bossac -i` probe result

2. SWD branch (preferred escape):
- External probe (CMSIS-DAP/J-Link) + Zephyr flash/debug
- Verify VM logs and peripheral tests over UART/RTT

## Sources

- Zephyr board docs: Arduino Nano 33 BLE
  - https://docs.zephyrproject.org/latest/boards/arduino/nano_33_ble/doc/index.html
- Zephyr issue history on Nano 33 BLE flashing quirks
  - https://github.com/zephyrproject-rtos/zephyr/issues/36610
- Arduino support: forcing bootloader mode by double-press reset
  - https://support.arduino.cc/hc/en-us/articles/5779192727068-Reset-your-board
