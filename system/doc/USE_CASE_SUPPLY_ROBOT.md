# OS/II Use Case: Autonomous Supply Vehicle

## The Setting

A hospital campus moves medication, lab samples, and sterile supplies
between buildings using small ground robots.  Each robot is a
four-wheeled platform the size of a toolbox, carrying a locked
compartment.  It navigates outdoor paths between buildings, following
painted lane markers, slowing for pedestrians, stopping for obstacles.

There is something endearing about a small machine that does one thing
faithfully.  It does not speak.  It does not have opinions.  It carries
what it is asked to carry, avoids what it is supposed to avoid, and
returns to its charging station when the battery runs low.  If you have
seen the Pixar film, you recognize the lineage — a compact, diligent
worker that finds purpose in a simple task, long after the systems
around it have grown complicated or failed entirely.

This one does not compact trash.  It delivers medication.  But it is clearly an ancestor: same stubborn reliability, same
indifference to the chaos above, same quiet return to the charging
dock when the day's work is done.  WALL-E's great-great-grandfather,
built decades before that yellow chassis — a LiPo cell instead of
solar panels, I2C instead of optical sensors, a BEAM-inspired runtime
on a chip smaller than a fingernail instead of whatever soul Pixar
imagined inside.  The family resemblance is unmistakable.

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
                    +--+-+-+-+-+-+-+-+
                       | | | | | | | |
            +----------+ | | | | | | +----------+
            |            | | | | | |            |
        bumper_L    imu  | | | | | tof_R    bumper_R
        (GPIO)    (I2C)  | | | | |  (I2C)   (GPIO)
                         | | | | |
                  motor_L  | | |  motor_R
                  (PWM)    | | |  (PWM)
                           | | |
                     batt_V  | i_sense_R
                     (ADC0)  |  (ADC2)
                             |
                       i_sense_L
                        (ADC1)
```

**I2C Sensors:**
- BMI270 IMU (bus 1, addr 0x68): acceleration, tilt, rollover detection
- VL53L0X time-of-flight (bus 1, addr 0x29): forward obstacle distance

**ADC Channels:**
- Channel 0: battery voltage via resistor divider (3.7V LiPo scaled
  to 0–3.3V).  Read once per second.
- Channels 1–2: motor current sensing via low-side shunt resistors
  (INA180 or similar).  Read at 100Hz.  A sudden current spike means
  the wheel hit something.  A gradual rise means the motor is
  straining — uphill, soft ground, or a box shifting in the
  compartment.
- Channel 3 (optional): ambient sound level from PDM microphone via
  the nRF52840's built-in PDM-to-ADC path.  A sustained sound above
  threshold — footsteps, a voice, a bicycle bell — triggers a
  slowdown before the distance sensor even sees the pedestrian.

**Actuators:**
- Two DC motors via H-bridge (PWM channels 0 and 1): left and right drive
- Status LED (GPIO): heartbeat / fault indicator

**GPIO:**
- Two bumper microswitches: physical contact, immediate motor stop

## The Flow

The engineer dictates:

> "Read the IMU at 50Hz for tilt.  Read distance at 20Hz.  Read motor
> current on both channels at 100Hz — if either exceeds 800mA, cut
> that motor to half duty.  Read battery once per second — if below
> 3.2 volts, reduce max speed to 30%.  Drive both motors from distance.
> If any sensor fails, stop motors."

A language model produces:

```erlang
#{sensors => [
    #{bus => 1, addr => 16#68, reg => 16#0C, poll_ms => 20},
    #{bus => 1, addr => 16#29, reg => 16#1E, poll_ms => 50},
    #{kind => adc, channel => 0, poll_ms => 1000},
    #{kind => adc, channel => 1, poll_ms => 10},
    #{kind => adc, channel => 2, poll_ms => 10}
  ],
  actuators => [
    #{kind => pwm, channel => 0},
    #{kind => pwm, channel => 1}
  ],
  flows => [
    #{from => 16#29, to => {pwm, 0}},
    #{from => 16#29, to => {pwm, 1}},
    #{from => {adc, 1}, to => {pwm, 0}, transform => current_limit},
    #{from => {adc, 2}, to => {pwm, 1}, transform => current_limit}
  ],
  policy => #{
    mailbox_depth => 32,
    watchdog_ms => 4000,
    on_fail => stop_actuator,
    battery_min_mv => 3200,
    current_limit_ma => 800
  }
}.
```

Seven processes at runtime:

| PID | Role | Rate | Source |
|-----|------|------|--------|
| 1 | IMU tilt monitor | 50Hz | I2C 0x68 |
| 2 | Distance ranger | 20Hz | I2C 0x29 |
| 3 | Battery monitor | 1Hz | ADC ch0 |
| 4 | Left motor current | 100Hz | ADC ch1 |
| 5 | Right motor current | 100Hz | ADC ch2 |
| 6 | Motor controller (left) | on message | PWM ch0 |
| 7 | Motor controller (right) | on message | PWM ch1 |

The combined sensor rate is 271Hz.  From the stress test, OS/II
handles 995 I2C events/s on this chip.  ADC reads are faster than
I2C (no bus protocol overhead).  The system is loaded to roughly
30% of its measured ceiling.  70% headroom remains for future
sensors or computation.

## What the ADC Channels See

### Battery (ADC Channel 0, 1Hz)

The LiPo cell voltage drops from 4.2V (full) to 3.0V (empty)
over a discharge curve.  A resistor divider scales this to the
nRF52840's 0–3.3V ADC range.  The battery process reads once
per second — fast enough to detect a low-battery condition but
slow enough to consume negligible CPU.

When the reading drops below 3.2V (configurable in the flow
policy), the battery process SENDs a throttle command to both
motor controllers: maximum duty capped at 30%.  The robot slows
down and conserves power for the return trip to its charging
station.  The Linux brain receives the battery level over BLE
and re-routes to the nearest charger.

If the battery drops below 3.0V, the battery process SENDs
duty=0 to both motors.  The robot stops where it is.  A
maintenance alert goes out over BLE.  The locked compartment
stays locked.

### Motor Current (ADC Channels 1–2, 100Hz)

A 0.1-ohm shunt resistor in each motor's ground return path
produces a voltage proportional to current.  An INA180 amplifier
scales this to the ADC range.  At 100Hz, the current process
captures transients that a 20Hz distance sensor would miss.

**Stall detection.**  If the robot's wheel catches on a curb edge
or a door threshold, current spikes from 200mA to 1.5A within
50ms.  The current-sense process detects this within one sample
(10ms) and SENDs a reduced duty to the motor controller.  The
wheel backs off before the motor overheats or the H-bridge
triggers thermal shutdown.

**Load monitoring.**  A gradual current increase from 200mA to
500mA over several seconds indicates the robot is climbing a ramp
or pushing through gravel.  The current process does not
intervene — this is normal operation.  But it SENDs the current
value to the Linux brain, which logs it for fleet maintenance
analytics.  A motor that consistently draws 20% more than its
peers across the fleet is developing bearing wear.

**Asymmetry detection.**  If the left motor draws 400mA and the
right draws 200mA at the same commanded duty, one wheel is
slipping or the load has shifted.  The current processes
independently SEND their readings.  The Linux brain, receiving
both, can detect the asymmetry and issue a steering correction.
The nRF52840 does not need to know about steering — it just
reports what it measures.

### Sound Level (ADC Channel 3, Optional)

The nRF52840 includes a PDM (Pulse Density Modulation) interface
connected to the onboard microphone on the Nano 33 BLE Sense.
The PDM peripheral decimates the microphone bitstream into PCM
samples without CPU intervention.  A sound-level process reads
the peak amplitude every 50ms.

This is not speech recognition.  It is a single number: loud or
quiet.  Footsteps on gravel produce a distinctive 200–400Hz
energy pattern.  A bicycle bell is a sharp spike above 1kHz.  A
delivery truck backing up is a sustained 1kHz beep.

When the sound level exceeds a threshold for three consecutive
readings (150ms), the sound process SENDs a slowdown command to
the motor controllers.  The robot reduces speed before the
distance sensor detects the pedestrian — because sound travels
around corners and through hedges, and infrared does not.

This is the cheapest pedestrian pre-warning system possible: one
microphone already on the board, one ADC channel, one process,
15 VM instructions per sample.  No neural network, no cloud
endpoint, no privacy concern.  Just: "it got louder, slow down."

## Why Process Isolation Matters Here

**Scenario 1: IMU cable vibrates loose.**

The IMU sensor process (pid 1) gets I2C NACKs.  It enters DEGRADED
with a 2-second backoff.  The other six processes keep running.  The
distance sensor still controls the motors.  The current monitors
still detect stalls.  The battery monitor still tracks voltage.  When
the cable reseats, the IMU recovers and resumes tilt monitoring.

In a single-threaded C firmware, a stuck I2C read blocks the main
loop.  All sensors stop.  The motors hold their last PWM duty until
the watchdog fires.

**Scenario 2: Motor stalls on a curb.**

The right motor current (pid 5) reads 1.5A — three times the limit.
It SENDs reduced duty to the right motor controller (pid 7) within
10ms.  The left motor (pid 6) keeps running at normal duty.  The
robot pivots slightly, the right wheel clears the curb, current
drops, and the right motor resumes full duty.

The distance sensor never saw the curb — it was below the beam angle.
The IMU registered a slight tilt but within normal bounds.  Only the
current sensor caught it, and only the right motor responded.

**Scenario 3: Battery fading on a hot afternoon.**

At 2:47 PM, after six delivery runs in 32-degree heat, the battery
process (pid 3) reads 3.18V — below the 3.2V threshold.  It SENDs
duty caps to both motor controllers.  The robot slows from 0.8 m/s
to 0.25 m/s.  The Linux brain receives the battery alert over BLE
and re-routes to the nearest charging pad, 40 meters away.

The robot arrives at 2:49 PM, docks, and begins charging.  At no
point did any process crash, any message drop, or any motor
misbehave.  The battery process did its job: one ADC read per
second, one comparison, one SEND when the threshold crossed.

**Scenario 4: Linux board crashes.**

The navigation brain ran out of memory during a TensorFlow model
reload.  The UART link goes silent.  The nRF52840 does not care —
the sensor processes keep running.  The distance sensor slows the
motors as obstacles appear.  The current monitors protect against
stalls.  The battery monitor tracks voltage.

The robot stops when it reaches an obstacle it cannot navigate
around without steering commands.  It holds position with motors
at zero duty, LED blinking the fault pattern, BLE advertising
"AWAITING_NAV" to any listener.  A maintenance technician walks
over, reboots the Linux board, and the robot resumes its route.

Total supply delivery delay: four minutes.  Total medication lost:
zero.  Total motors damaged: zero.

## The Numbers

| Property | Value |
|----------|-------|
| Sensor processes | 5 (IMU, ToF, battery, 2x current) |
| Actuator processes | 2 (left motor, right motor) |
| Combined sensor rate | 271 Hz |
| System ceiling (measured) | 995 events/s |
| Load factor | 27% |
| Motor response to current spike | <10ms (1 ADC sample) |
| Battery poll interval | 1 second |
| Mailbox depth at steady state | 0/32 |
| RAM used | 42 KB of 256 KB (16%) |
| RAM free for future use | 214 KB |
| Flash used | 62 KB of 928 KB (7%) |
| Watchdog recovery time | <5 seconds |

## What It Takes to Build This

OS/II today has the VM, scheduler, GC, and flow compiler.  The
supply robot needs three additions:

1. **ADC BIF** (`MB_BIF_ADC_READ`): read one ADC channel, return
   raw 12-bit value.  The nRF52840 SAADC is a single-register
   peripheral — the BIF is approximately 20 lines of C.

2. **Policy enforcement in the actuator process**: when a fault
   flag is set (sensor DEGRADED or battery low), override the
   commanded duty to zero or a reduced cap.  This is bytecode
   logic, not a runtime change.

3. **UART BIF** for receiving navigation commands from the Linux
   brain.  A UART-reader process bridges the serial link to the
   mailbox system.

None of these require changes to the VM interpreter, the scheduler,
the GC, or the flow compiler.  They are BIF additions (new hardware
operations) and flow-level logic (new process programs).

The architecture is load-bearing.  The additions are drywall.

## The Point

A small robot carrying medicine across a hospital campus does not need
a large runtime.  It needs processes that cannot interfere with each
other, a mailbox that tells you when it is full, a garbage collector
that does not stop the world, and a watchdog that reboots when
everything else fails.

The nRF52840 has eight ADC channels, four PWM outputs, two I2C buses,
Bluetooth, and a microphone.  OS/II turns each peripheral into a
process and each wire into a message.  The engineer writes an Erlang
term describing what connects to what.  The compiler does the rest.

The robot does not know it is running a BEAM-inspired runtime.  It
knows that when the battery is low, it slows down.  When a wheel
stalls, it backs off.  When it hears footsteps, it yields the path.
When the brain crashes, it stops and waits.

That is enough.  That has always been enough.
