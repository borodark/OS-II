# Zephyr App (nRF52840)

This app runs the mini VM on nRF52840 using Zephyr/NCS drivers.

## Bootstrap (fresh machine)

```bash
./bootstrap_zephyr_workspace.sh "$HOME/zephyrproject"
```

Then:

```bash
source "$HOME/zephyrproject/.venv/bin/activate"
export ZEPHYR_WS="$HOME/zephyrproject"
```

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

## First-Pass Resilience Test

Track C first pass adds retry/degraded/recovered status transitions and optional
synthetic fault injection.

Build with fault injection every 5th read:

```bash
west build -b arduino_nano_33_ble/nrf52840/sense -p always \
  -- -DOS2_FAULT_EVERY_N=5
```

Or using the project script:

```bash
OS2_FAULT_EVERY_N=5 ./reflash_nano33_sense.sh --monitor
```

Runtime event logs include:
- `status=4` (`RETRYING`)
- `status=5` (`DEGRADED`)
- `status=6` (`RECOVERED`)
- `inj=1` for synthetic injected faults

If a sensor stays degraded past grace window, the task watchdog intentionally
stops being fed and performs a cold reboot recovery.

Board debug instructions:
- `README_DEBUG_BOARD.md`

Notes:
- HAL implementation uses `led0` alias for GPIO, `i2c0` alias for I2C, and `pwm0` node label for PWM.
- Build is configured with strict checks (`MB_STRICT_DTS`) and fails if required aliases are missing.
- Board overlays are provided in `boards/` to pin these aliases for supported boards.
