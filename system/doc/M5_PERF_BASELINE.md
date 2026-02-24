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
```

## Baseline Snapshot (2026-02-24)

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

## Next M5 Steps

1. Run a longer nominal soak (>= 10 min) to quantify drift and long-tail jitter.
2. Add p50/p95/p99 for end-to-end inter-event deltas across all sensors.
3. Add CSV/JSON output mode for analyzer to feed plotting notebooks.
