# M5 Performance Baseline

This document starts M5 with repeatable, log-based performance measurement.

## Tool

- Analyzer: `erts/example/mini_beam_esp32/zephyr_app/analyze_event_perf.sh`
- Input: serial log file containing `event ... ts=...` lines.

Run:

```bash
cd erts/example/mini_beam_esp32/zephyr_app
./analyze_event_perf.sh logs/fault_test.log
./analyze_event_perf.sh logs/recovery_evidence.log
./analyze_event_perf.sh logs/nominal_soak_10m.log --scenario nominal_soak_10m --csv ../../../system/doc/M5_BASELINE_NOMINAL_SOAK.csv
./analyze_event_perf.sh logs/nominal_soak_10m.log --scenario nominal_soak_10m --json logs/nominal_soak_10m.json
./analyze_event_perf.sh logs/recovery_evidence.log --scenario recovery_evidence --csv ../../../system/doc/M5_BASELINE_RECOVERY.csv
```

## Baseline Snapshot (2026-02-24)

### 10-minute nominal soak (`logs/nominal_soak_10m.log`)

- `events_total=1775`
- `duration_ms=800008`
- `event_rate_hz=2.219`
- `inj_events=0`
- `status_0=1775`
- per-sensor average period:
  - `sensor_1_avg_ms=1352.73`
  - `sensor_2_avg_ms=1352.70`
  - `sensor_3_avg_ms=1015.83`
- per-sensor period jitter (ms):
  - `sensor_1 p50/p95/p99 = 1025/1026/1026`
  - `sensor_2 p50/p95/p99 = 1025/1026/1026`
  - `sensor_3 p50/p95/p99 = 1006/1026/1026`
- mailbox correlation:
  - first stats: `attempted=849 processed=849`
  - last stats: `attempted=2619 processed=2619`
  - `mb_processed_over_attempted_pct=100.00`
  - `mb_drop_over_attempted_pct=0.00`
  - `mb_delta_attempted=1770`, `mb_delta_processed=1770`

### Nominal run (`logs/nominal_perf.log`)

- `events_total=102`
- `duration_ms=34076`
- `event_rate_hz=2.993`
- `inj_events=0`
- `status_0=102`
- per-sensor average period:
  - `sensor_1_avg_ms=1016.12`
  - `sensor_2_avg_ms=1015.52`
  - `sensor_3_avg_ms=1016.12`
- per-sensor period jitter (ms):
  - `sensor_1 p50/p95/p99 = 1025/1026/1026`
  - `sensor_2 p50/p95/p99 = 1006/1026/1026`
  - `sensor_3 p50/p95/p99 = 1025/1026/1026`
- mailbox correlation:
  - first stats: `attempted=159 processed=159`
  - last stats: `attempted=249 processed=249`
  - `mb_processed_over_attempted_pct=100.00`
  - `mb_drop_over_attempted_pct=0.00`
  - `mb_delta_attempted=90`, `mb_delta_processed=90`

### Forced-failure recovery run (`logs/recovery_evidence.log`)

- `events_total=57`
- `duration_ms=16881`
- `event_rate_hz=3.377`
- `inj_events=57`
- `status_4=12`, `status_5=45`
- per-sensor average period:
  - `sensor_1_avg_ms=773.94`
  - `sensor_2_avg_ms=773.89`
  - `sensor_3_avg_ms=773.89`
- per-sensor period jitter (ms):
  - `sensor_1 p50/p95/p99 = 2006/2027/2027`
  - `sensor_2 p50/p95/p99 = 2007/2027/2027`
  - `sensor_3 p50/p95/p99 = 2007/2027/2027`
- mailbox correlation:
  - first stats: `attempted=12 processed=12`
  - last stats: `attempted=21 processed=21`
  - `mb_processed_over_attempted_pct=100.00`
  - `mb_drop_over_attempted_pct=0.00`
  - `mb_delta_attempted=9`, `mb_delta_processed=9`

## Regression Gate (CSV)

Use CSV output with a deterministic threshold check:

```bash
cd erts/example/mini_beam_esp32/zephyr_app
./check_perf_regression.sh ../../../system/doc/M5_BASELINE_NOMINAL_SOAK.csv \
  --scenario nominal_soak_10m \
  --min-event-rate-hz 2.0 \
  --max-drop-pct 0.10 \
  --min-processed-pct 99.0 \
  --max-sensor-p99-ms 1300
```

CI workflow (`.github/workflows/perf-gate.yml`) runs this gate on each push/PR.

## Soak Profiles

Use `run_soak_profile.sh` for repeatable long-run captures:

```bash
cd erts/example/mini_beam_esp32/zephyr_app
./run_soak_profile.sh --profile 10m --sudo-chown
./run_soak_profile.sh --profile 30m --sudo-chown
./run_soak_profile.sh --profile 60m --sudo-chown
```

Outputs per profile:
- `logs/nominal_soak_<profile>.log`
- `logs/nominal_soak_<profile>.csv`
- `logs/nominal_soak_<profile>.json`

Reference runbook: `system/doc/M5_SOAK_PROFILES.md`

## Next M5 Steps

1. Capture and commit a 30-minute baseline CSV/JSON from hardware.
2. Capture and commit a 60-minute baseline CSV/JSON from hardware.
3. Extend checks with global inter-event delta p95/p99 across all sensors.
