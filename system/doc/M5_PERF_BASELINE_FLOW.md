# M5 Performance Baseline — Two-Process Flow (2026-03-29)

## Configuration

- Board: Arduino Nano 33 BLE Sense (nRF52840 @ 64MHz, 256KB RAM)
- Firmware: flow-compiled two-process scheduler
- Processes: sensor (pid=1) + actuator (pid=2)
- Flow: APDS9960 I2C read → SEND → PWM set duty
- Poll interval: 200ms (from `nano33_sensor_pwm.flow`)
- Reductions per tick: 64
- Watchdog: disabled for measurement
- Duration: 30 seconds (149 events captured)

## Timing

| Metric | Value |
|--------|-------|
| Event rate | 5.0 events/s (target: 5.0) |
| Period mean | 202.5ms |
| Period median | 202.0ms |
| Period stdev | 0.8ms |
| Period min | 202ms |
| Period max | 206ms |
| P95 | 204ms |
| P99 | 206ms |
| Jitter (max-min) | 4ms |

The 2.5ms overshoot from the 200ms target is the I2C transaction time
(~0.2ms per read at 400kHz) plus scheduler overhead (~2ms for
tick + wake + dispatch).

## Scheduler

| Metric | Value |
|--------|-------|
| Total ticks | 1223 in 30s |
| Tick rate | 41 ticks/s |
| Ticks per event | 8.2 |
| Idle ticks | ~776 (main loop sleeps 10ms) |
| Active ticks | ~447 |

Most ticks are idle (both processes sleeping/waiting). Active ticks
execute the sensor's I2C BIF + SEND sequence and the actuator's
RECV_CMD + PWM BIF sequence.

## Mailbox

| Metric | Value |
|--------|-------|
| Depth | 0/32 (always drained) |
| Drops | 0 |
| Policy | reject_new |

Messages are consumed within the same tick they arrive. The actuator
wakes immediately when the sensor SENDs.

## Memory

| Region | Used | Total | % |
|--------|------|-------|---|
| RAM | 42KB | 256KB | 16.4% |
| Flash | 62KB | 928KB | 6.5% |
| Heap GC | 0 cycles | — | — |

No heap allocation in the current flow (only smallint registers).
GC overhead is zero for this workload.

## Process States (Steady-State)

- Sensor (pid=1): SLEEPING between polls, wakes every 200ms
- Actuator (pid=2): WAITING on RECV_CMD, wakes when sensor SENDs

## Comparison with Single-Process Baseline

The previous single-process architecture used native C to enqueue
commands and run the VM. The two-process flow achieves identical
timing accuracy (±4ms jitter) with the scheduling overhead absorbed
into the 200ms poll interval.

## Regression Gate Thresholds

Based on this baseline, proposed gates for CI:

- Event rate: >= 4.5 events/s (allow 10% degradation)
- Period P99: <= 220ms (allow 10% over target)
- Mailbox drops: 0% at steady state
- Jitter: <= 10ms
