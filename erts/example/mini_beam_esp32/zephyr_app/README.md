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

Capture full degraded/reboot evidence:

```bash
./capture_recovery_evidence.sh logs/recovery_evidence.log
```

Validate an existing log:

```bash
./check_recovery_log.sh logs/recovery_evidence.log
```

M5 performance baseline from log data:

```bash
./analyze_event_perf.sh logs/recovery_evidence.log
./analyze_event_perf.sh logs/nominal_soak_10m.log --scenario nominal_soak_10m --csv ../../../system/doc/M5_BASELINE_NOMINAL_SOAK.csv
./analyze_event_perf.sh logs/nominal_soak_10m.log --scenario nominal_soak_10m --json logs/nominal_soak_10m.json
./perf_gate_rc.sh
```

Long soak profile helper (captures + analyzes + gates):

```bash
./run_soak_profile.sh --profile 30m --sudo-chown
./run_soak_profile.sh --profile 60m --sudo-chown
./promote_soak_baseline.sh --profile 30m
./promote_soak_baseline.sh --profile 60m
```

Parallel multi-board soaks:

```bash
./run_parallel_soaks.sh --profile 30m --sudo-chown \
  --ports /dev/ttyACM0,/dev/ttyACM1,/dev/ttyACM2
```

Optional first deployment before parallel capture:

```bash
./run_parallel_soaks.sh --profile 10m --sudo-chown --flash-first
```

M6 first pass:
- VM mailbox now handles both `MB_CMD_I2C_READ` and `MB_CMD_PWM_SET_DUTY` in one cyclic program.
- Runtime emits actuator logs:
  - `event kind=actuator op=pwm_set_duty actuator_id=... channel=... duty_permille=... value=...`

P0 capability schema lock:
- Boot emits one Erlang-term capability map line:
  - `os2_caps_v1 #{caps_v=>1,...}`
- Validate captured logs:

```bash
./validate_caps_term.sh logs/nano33.log
```

P1 profile + binding validation:
- Profile file:
  - `profiles/nano33_ble_sense.os2`
- Validate profile requirements and logical bindings against boot caps term:

```bash
./validate_profile_bindings.sh \
  --profile profiles/nano33_ble_sense.os2 \
  --log logs/nano33.log
```

Runtime preflight guard (fail-fast):
- `redeploy_and_log.sh` and `run_soak_profile.sh` now run a preflight by default.
- Preflight checks:
  1) `os2_caps_v1` exists and is valid.
  2) profile bindings are compatible with capabilities.
- Override only when needed:
  - `--skip-preflight`

Manual preflight command:

```bash
./preflight_profile_check.sh \
  --profile profiles/nano33_ble_sense.os2 \
  --port /dev/ttyACM0
```

Board debug instructions:
- `README_DEBUG_BOARD.md`

Notes:
- HAL implementation uses `led0` alias for GPIO, `i2c0` alias for I2C, and `pwm0` node label for PWM.
- Build is configured with strict checks (`MB_STRICT_DTS`) and fails if required aliases are missing.
- Board overlays are provided in `boards/` to pin these aliases for supported boards.
