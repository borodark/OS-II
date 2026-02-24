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

### Fault-injection every 5th read (`logs/fault_test.log`)

- `events_total=72`
- `duration_ms=23897`
- `event_rate_hz=3.013`
- `inj_events=12`
- `status_0=60`, `status_4=12`
- per-sensor average period:
  - `sensor_1_avg_ms=1015.39`
  - `sensor_2_avg_ms=1016.22`
  - `sensor_3_avg_ms=1015.35`

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

## Next M5 Steps

1. Add nominal no-fault run (`OS2_FAULT_EVERY_N=0`) as primary throughput baseline.
2. Capture jitter distribution (p50/p95/p99 period) per sensor.
3. Correlate `mb_stats` throughput with event timing under load.
