# M5 Soak Profiles (10m/30m/60m)

Use this command from repo root:

```bash
./erts/example/mini_beam_esp32/zephyr_app/run_soak_profile.sh --profile 10m --sudo-chown
./erts/example/mini_beam_esp32/zephyr_app/run_soak_profile.sh --profile 30m --sudo-chown
./erts/example/mini_beam_esp32/zephyr_app/run_soak_profile.sh --profile 60m --sudo-chown
```

Generated artifacts:
- `erts/example/mini_beam_esp32/zephyr_app/logs/nominal_soak_<profile>.log`
- `erts/example/mini_beam_esp32/zephyr_app/logs/nominal_soak_<profile>.csv`
- `erts/example/mini_beam_esp32/zephyr_app/logs/nominal_soak_<profile>.json`

Suggested acceptance checks:
- `event_rate_hz >= 2.0`
- `mb_drop_over_attempted_pct <= 0.10`
- `mb_processed_over_attempted_pct >= 99.0`
- `sensor_1_p99_ms <= 1300`
- `sensor_2_p99_ms <= 1300`
- `sensor_3_p99_ms <= 1300`

Promote stable runs by copying CSV/JSON into `system/doc/` with date/profile in filename.
