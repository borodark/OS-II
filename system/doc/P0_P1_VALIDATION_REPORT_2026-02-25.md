# P0/P1 Validation Report (2026-02-25)

## Scope

This report captures proof that:

1. `P0` is active on hardware: firmware emits `os2_caps_v1` as an Erlang map term at boot.
2. `P1` profile/binding validation passes against real board capabilities from runtime logs.

Target board:
- Arduino Nano 33 BLE Sense (`arduino_nano_33_ble_sense`)

Log source:
- `erts/example/mini_beam_esp32/zephyr_app/logs/nano33.log`

## Results

`P0`: PASS
- `validate_caps_term.sh` found and validated:
  - `os2_caps_v1 #{caps_v=>1,...}`
- Capability payload includes:
  - `event_schema=>2`
  - `mailbox_depth=>32`
  - `i2c=>2`, `pwm=>4`, `adc=>#{channels=>8,max_ksps=>200}`
  - `policy=>reject_new`
  - `power_domains=>[vdd_env,mic_pwr]`

`P1`: PASS
- `validate_profile_bindings.sh` validated:
  - profile: `profiles/nano33_ble_sense.os2`
  - log: `logs/nano33.log`
- Deterministic binding table:
  - `i2c_bus=1`
  - `pwm_channel=0`
  - `sensor_addr=0x39`
  - `sensor_reg=0x92`
  - `pwm_freq_hz=20000`

## Evidence Files

- proof bundle: `system/doc/P0_P1_PROOF_2026-02-25.txt`
- profile used: `erts/example/mini_beam_esp32/zephyr_app/profiles/nano33_ble_sense.os2`
- validators:
  - `erts/example/mini_beam_esp32/zephyr_app/validate_caps_term.sh`
  - `erts/example/mini_beam_esp32/zephyr_app/validate_profile_bindings.sh`

## Conclusion

`P0` and `P1` are both validated on hardware and reproducible from captured runtime logs.
