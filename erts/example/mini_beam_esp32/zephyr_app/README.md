# Zephyr App (nRF52840)

This app runs the mini VM on nRF52840 using Zephyr/NCS drivers.

## Build

Typical board targets:
- `nrf52840dk/nrf52840`
- `arduino_nano_33_ble`

```bash
cd erts/example/mini_beam_esp32/zephyr_app
west build -b nrf52840dk/nrf52840
west flash
west debug
```

For Nano 33 BLE:

```bash
west build -b arduino_nano_33_ble
```

Board debug instructions:
- `README_DEBUG_BOARD.md`

Notes:
- HAL implementation uses `led0` alias for GPIO, `i2c0` alias for I2C, and `pwm0` node label for PWM.
- Build is configured with strict checks (`MB_STRICT_DTS`) and fails if required aliases are missing.
- Board overlays are provided in `boards/` to pin these aliases for supported boards.
