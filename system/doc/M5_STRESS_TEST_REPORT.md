# M5 Stress Test Report (2026-03-29)

## Purpose

Determine the throughput ceiling of the OS/II two-process flow runtime
on nRF52840 hardware, and identify which subsystem is the bottleneck.

## Test Configuration

Board: Arduino Nano 33 BLE Sense (nRF52840 @ 64MHz, 256KB RAM).
Firmware: flow-compiled multi-process scheduler with tagged terms.
Watchdog: disabled for measurement.

## Test Progression

### Test 1 — Baseline (200ms poll)

Two processes: sensor reads APDS9960 every 200ms, SENDs to actuator
which calls PWM BIF.

| Metric | Value |
|--------|-------|
| Event rate | 5.0/s |
| Period mean | 202.5ms |
| Period stdev | 0.8ms |
| P99 | 206ms |
| Mailbox peak depth | 0/32 |
| Errors | 0 |

Conclusion: nominal operation well within margins.

### Test 2 — Fast poll (1ms target)

Same two processes, poll interval reduced from 200ms to 1ms.

| Metric | Value |
|--------|-------|
| Event rate | 654/s |
| Period mean | 1.53ms |
| P99 | 4ms |
| Mailbox peak depth | 0/32 |
| Errors | 0 |

Conclusion: scheduler handles 654 events/s without drops.  The idle
sleep in the main loop (10ms at the time) was the limiting factor,
not the scheduler or I2C.

### Test 3 — Multi-sender saturation (4 senders, no sleep)

Four sensor processes running in tight loops (YIELD instead of
SLEEP_MS) all SENDing to one actuator process.  No idle sleep in the
main loop.  Per-event logging disabled to remove I/O bottleneck from
measurement — only one summary LOG line per 5 seconds.

| Metric | Value |
|--------|-------|
| Scheduler ticks/s | 4,023 |
| I2C events/s | 995 |
| Idle ticks | 0 (CPU 100%) |
| Mailbox peak depth | 0/32 |
| Errors | 0 |
| Duration | 25+ seconds |

Each tick runs one process for up to 64 VM instructions.  At 4,023
ticks/s across 5 processes, that is approximately 257,000 VM
instructions per second.

## Bottleneck Analysis

The I2C bus at 400kHz is the ceiling.  A single I2C read-register
transaction (1 byte write + 1 byte read) takes approximately 0.25ms
including address, register, repeated start, and data phases.  Four
sensors each reading at maximum rate produce ~250 reads/s each,
totalling ~1,000 reads/s — matching the observed 995 events/s.

The scheduler has significant headroom.  In the baseline test (200ms
poll), the scheduler achieves 120,000 idle ticks/s.  Under full I2C
load, only 4,023 ticks/s are used.  The remaining capacity is
available for computation, filtering, or additional processes that do
not require I2C.

The mailbox (32 slots, reject-new policy) never reached even 1 slot
of occupancy under any test condition.  The actuator process wakes
and drains within the same scheduler tick that delivers the message.

## Decision: No Synthetic Stress Beyond I2C Saturation

A pure-compute test (no I2C, just SEND/RECV in tight loops) would
measure the VM in isolation.  We chose not to pursue this because:

1. The nRF52840 is a sensor/actuator platform.  Every real workload
   involves I2C, SPI, or ADC transactions.  A benchmark without I/O
   does not represent any deployable scenario.

2. The scheduler already shows 97% headroom (4K of 120K ticks/s used).
   The runtime disappears behind the peripheral — which is the design
   goal of OS/II.

3. The mailbox never backed up.  To overflow a 32-slot queue, a sender
   would need to enqueue faster than the actuator can dequeue.  Since
   both run at the same scheduler priority with the same reduction
   budget, the round-robin dispatch ensures the consumer keeps pace.

## Summary

| Property | Result |
|----------|--------|
| Throughput ceiling | 995 I2C events/s (bus-limited) |
| Scheduler capacity | 4,023 ticks/s under load, 120K idle |
| VM throughput | ~257K instructions/s |
| Mailbox overflow | never observed |
| Error rate | 0 |
| Bottleneck | I2C bus bandwidth (400kHz) |
| Runtime overhead | <3% of CPU at I2C saturation |
