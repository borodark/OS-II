# OS/II Board Profile Mapping (v1)

## Logical-to-Hardware Mapping

Logical channel semantics used by VM commands:

- GPIO pin `0` => `DT_ALIAS(led0)`
- PWM channel `0` => `pwm0` device node label
- I2C bus `0` => `DT_ALIAS(i2c0)` (fallback node label `i2c0`)

## Supported Board Profiles

1. `nrf52840dk_nrf52840`
- Overlay file: `erts/example/mini_beam_esp32/zephyr_app/boards/nrf52840dk_nrf52840.overlay`

2. `arduino_nano_33_ble`
- Overlay file: `erts/example/mini_beam_esp32/zephyr_app/boards/arduino_nano_33_ble.overlay`

## Programming/Debug Transports

1. `nrf52840dk_nrf52840`
- Preferred transport: onboard SWD/J-Link.
- Risk level: low for repeated flash/debug cycles.

2. `arduino_nano_33_ble`
- Default transport: USB bootloader (`bossac`).
- Required tool choice: Arduino-packaged `bossac` is preferred.
- Risk level: medium (bootloader timing/re-enumeration sensitivity).
- Fallback: external SWD probe for reliable flashing/debugging.

## Rule

Do not change logical channel numbers in VM contract. Adapt board overlays instead.
