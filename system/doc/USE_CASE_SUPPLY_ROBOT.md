# OS/II Use Case: Autonomous Supply Vehicle

## The Setting

A hospital campus moves medication, lab samples, and sterile supplies
between buildings using small ground robots.  Each robot is a
four-wheeled platform the size of a toolbox, carrying a locked
compartment.  The robots navigate outdoor paths between buildings,
following painted lane markers and avoiding pedestrians.

A Linux single-board computer (Raspberry Pi or Jetson) handles
navigation: camera processing, path planning, SLAM.  But the Linux
board does not directly control the motors or read the safety sensors.
Between the navigation brain and the physical world sits an nRF52840.

## Why a Dedicated Motor Controller

The Linux board runs a full OS.  It garbage-collects, swaps, schedules
hundreds of threads, handles network interrupts.  Its worst-case
response time to a GPIO edge is measured in milliseconds — sometimes
tens of milliseconds.  This is fine for planning a route.  It is not
fine for stopping a motor when a bumper switch closes.

The nRF52840 runs at 64MHz with 256KB of RAM and nothing else.  No
filesystem, no network stack, no display server.  Its job is to be
fast, predictable, and always running.  If the Linux board freezes
during a kernel update, the nRF52840 keeps the motors safe.

## The Hardware

```
                    +-----------------+
                    |   Linux SBC     |
                    |  (navigation)   |
                    +--------+--------+
                             | UART / BLE
                    +--------+--------+
                    |    nRF52840     |
                    |    (OS/II)      |
                    +--+-+-+-+-+-+---+
                       | | | | | |
            +----------+ | | | | +----------+
            |            | | | |            |
        bumper_L     imu | | | tof_R     bumper_R
        (GPIO)    (I2C)  | |  (I2C)     (GPIO)
                         | |
                    motor_L  motor_R
                    (PWM)    (PWM)
```

**Sensors:**
- BMI270 IMU (I2C bus 1, addr 0x68): acceleration, tilt, rollover detection
- VL53L0X time-of-flight ranging (I2C bus 1, addr 0x29): forward obstacle distance
- Two bumper microswitches (GPIO): physical contact detection

**Actuators:**
- Two DC motors via H-bridge (PWM channels 0 and 1): left and right drive
- Status LED (GPIO): heartbeat / fault indicator

## The Flow

The engineer dictates to her terminal:

> "Read the IMU every 20 milliseconds for tilt.  Read the distance
> sensor every 50 milliseconds.  Drive both motors from the distance
> reading — slow down when something is close.  If any sensor fails,
> stop both motors."

A language model produces:

```erlang
#{sensors => [
    #{bus => 1, addr => 16#68, reg => 16#0C, poll_ms => 20},
    #{bus => 1, addr => 16#29, reg => 16#1E, poll_ms => 50}
  ],
  actuators => [
    #{kind => pwm, channel => 0},
    #{kind => pwm, channel => 1}
  ],
  flows => [
    #{from => 16#29, to => {pwm, 0}},
    #{from => 16#29, to => {pwm, 1}}
  ],
  policy => #{
    mailbox_depth => 32,
    watchdog_ms => 4000,
    on_fail => stop_actuator
  }
}.
```

The flow compiler runs on the engineer's laptop:

```
$ escript flow_compile.escript supply_robot.flow flow_generated.h
flow_compile: supply_robot.flow -> flow_generated.h
  sensor 1: 72 bytes    (IMU @ 50Hz)
  sensor 2: 72 bytes    (ToF @ 20Hz)
  actuator:  17 bytes   (PWM receiver)
  processes: 3
```

Three processes.  The IMU sensor runs at 50Hz, the distance sensor at
20Hz, and the actuator receives from both.  Total bytecode: 161 bytes.

## What Runs on the nRF52840

At boot, OS/II:

1. Emits the capability schema:
   `os2_caps_v1 #{board=>supply_ctrl,vm=>mini_beam,i2c=>2,pwm=>4,...}`

2. Probes I2C bus 1 with timeout-guarded threads.  The BMI270 responds
   at 0x68.  The VL53L0X responds at 0x29.  Both are registered as
   sensor targets.

3. Spawns three processes from the compiled flow:
   - **pid 1** (IMU sensor): reads accelerometer register every 20ms,
     SENDs tilt value to pid 3.
   - **pid 2** (distance sensor): reads ranging register every 50ms,
     SENDs distance value to pid 3.
   - **pid 3** (motor actuator): blocks on RECV_CMD, sets PWM duty
     on both channels proportional to distance.

4. Enters the scheduler loop.  The main thread ticks the scheduler
   and feeds the watchdog.

The scheduler runs at approximately 4,000 ticks per second.  At each
tick, it picks a READY process and runs it for up to 64 VM
instructions.  The IMU sensor wakes every 20ms, reads one I2C
register, sends a command, and goes back to sleep — about 15
instructions.  The distance sensor does the same every 50ms.  The
actuator wakes on each message, calls the PWM BIF, and blocks again.

Between ticks, 97% of the CPU is idle.  The nRF52840 can enter
low-power idle and wake on the next kernel timer tick.

## Why Process Isolation Matters Here

**Scenario 1: IMU cable vibrates loose.**

The IMU sensor process (pid 1) reads I2C and gets -5 (NACK).  Its
internal retry counter increments.  After two retries, the process
enters DEGRADED state with a 2-second backoff.  Meanwhile, the
distance sensor (pid 2) and the motor actuator (pid 3) continue
running normally.  The motors keep driving based on distance data.
The IMU process retries every 2 seconds.  When the cable reseats
from vibration, the next read succeeds, the process transitions to
RECOVERED, and tilt monitoring resumes.

In a single-threaded C firmware, a stuck I2C read would block the
main loop.  All sensors would stop.  The motors would hold their
last PWM value until the watchdog fires.

**Scenario 2: Obstacle appears suddenly.**

The distance sensor (pid 2) reads 150mm.  It SENDs `PWM_SET_DUTY`
with a low duty value to pid 3.  The actuator receives within the
same scheduler tick — the mailbox depth never exceeds 0/32 under
normal operation.  Both motors decelerate.  The next read, 50ms
later, shows 80mm.  Duty drops further.  The robot slows to a stop.

The Linux navigation brain has not yet processed the camera frame.
It will catch up in 200ms and plan a new route.  But the motors are
already safe.

**Scenario 3: Linux board crashes.**

The navigation brain was running a Python ML model and ran out of
memory.  The UART link goes silent.  The nRF52840 does not care — it
was never receiving commands over UART in this flow.  The sensor
processes keep running, the motors keep responding to distance
readings.  The robot stops when it reaches an obstacle and holds
position.

The watchdog is set to 4 seconds.  If the nRF52840 itself hangs —
say, a bug in the VM — the watchdog fires and the board cold-reboots.
On reboot, the flow restarts from the compiled bytecode.  Time from
fault to recovery: under 5 seconds.

## The Numbers

From actual hardware measurements on the same nRF52840 SoC:

| Property | Value |
|----------|-------|
| IMU poll rate | 50Hz (20ms) |
| Distance poll rate | 20Hz (50ms) |
| Period jitter | <4ms |
| Motor response time | <1 scheduler tick (<0.25ms) |
| Mailbox depth at steady state | 0/32 |
| CPU used by scheduler | <3% |
| RAM | 42KB of 256KB (16%) |
| Flash | 62KB of 928KB (7%) |
| Remaining RAM for navigation buffers | 214KB |
| Recovery time (watchdog reboot) | <5 seconds |
| I2C throughput ceiling | 995 events/s |

The supply robot uses two sensors at a combined 70Hz.  The runtime
is loaded to 1% of its measured ceiling.

## What Changes to Deploy This

OS/II today proves the runtime on real I2C sensors and real PWM
outputs.  To deploy on a supply robot, the following additions are
needed:

**P3 policy enforcement.**  The flow declares `on_fail =>
stop_actuator` but the runtime does not yet zero the PWM on sensor
failure.  This is a small addition to the actuator process: a
fault-state register that, when set, overrides duty to zero.

**UART command channel.**  The Linux navigation brain sends
steering commands (target speeds for left and right motors) over
UART.  This requires a new BIF (`UART_READ`) and a third sensor
process that reads UART and SENDs speed targets to the actuator.
The actuator would blend navigation commands with safety overrides
from the distance sensor.

**GPIO bumper interrupt.**  The bumper switches need immediate motor
stop — faster than a polled I2C read.  This requires a GPIO interrupt
handler in the HAL that directly zeroes PWM and sets a fault flag,
bypassing the VM entirely.  The VM's job is to detect the fault flag
on its next tick and transition to a safe state.

**Battery monitor.**  An ADC read of the battery voltage, once per
second, reported via BLE to the base station.  Requires ADC BIF
and BLE service — both are nRF52840 capabilities already in the
capability schema.

None of these require changes to the VM, scheduler, GC, or flow
compiler.  They are BIF additions (UART_READ, ADC_READ) and policy
logic in the actuator process.

## The Point

The nRF52840 is not a big computer pretending to be small.  It is a
small computer doing exactly what small computers do: reading sensors
and driving actuators with predictable timing.

OS/II adds one thing that bare-metal C does not: **isolation**.  When
the IMU fails, the motors do not stop.  When the distance sensor
reports danger, the motors respond in under a millisecond.  When the
Linux brain crashes, the robot does not drive off a curb.

The cost of this isolation is 42KB of RAM and 3% of CPU.  On a chip
with 256KB and 97% idle time, that cost is invisible.

The supply robot does not need Erlang's distributed protocols or hot
code loading.  It needs processes that do not interfere with each
other, mailboxes that provide backpressure, and a watchdog that
reboots when everything else fails.  That is what BEAM is, stripped
to its load-bearing structure.  That is OS/II.
