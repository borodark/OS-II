# The Soul of OS/II, Part I

A development tribute in the spirit of *The Soul of a New Machine*.

## 1. The Workbench at 02:00

The room is not quiet. Three USB cables hum through a powered hub, and each cable ends in the same shape of board, each with a different temperament.

One board flashes on the first try.
One board needs the ritual: double-tap reset, watch for the breathing LED, race the bootloader.
One board pretends to be compliant until the exact second the toolchain asks for certainty.

What we learned first was not about elegance. It was about timing windows, descriptor errors, and ownership races in `/dev/ttyACM*`. The machine would not negotiate with intent; it negotiated with concrete steps.

## 2. A Smaller Promise

The original ambition was large: porting ERTS/BEAM concepts toward microcontroller reality.
The practical contract became smaller and stronger:

- bounded mailbox
- register VM
- HAL-only side effects
- fixed event schema
- watchdog-backed recovery

This reduction was not retreat. It was architecture under pressure. We chose determinism over breadth, and observability over mythology.

## 3. Three Kinds of Progress

Progress came in three tracks:

1. Runtime behavior
- VM loop executes sensor and actuator paths.
- Mailbox backpressure policy is explicit and measurable.
- Degraded mode leads to deliberate watchdog reboot.

2. Tooling behavior
- reflash scripts for Nano 33 BLE/Sense
- reconnecting serial logger
- soak harnesses for 10m/30m/60m
- regression gates from captured CSV baselines

3. Team behavior
- naming and labeling boards by serial
- codifying reset/flash rituals
- replacing folklore with scripts

## 4. The Event Contract as Culture

The event line became the cultural center of the project.

If a line is emitted, it must answer:

- what kind (`sensor` or `actuator`)
- what operation (`i2c_read`, `pwm_set_duty`)
- what value and return code
- what status and timestamp

When logs become machine-readable, arguments about “what happened” get replaced by evidence.

## 5. Why This Feels Like Kidder Territory

Kidder wrote about people assembling identity through engineering constraints. This project has the same geometry at a different scale:

- the spec is unstable until reality hardens it
- the winning idea is often the one that survives contact with hardware
- the team narrative is encoded in command history and crash loops

The modern equivalent of a war room is a shell prompt, a tailing log, and a diff that explains itself.

## 6. Are We Halfway?

Approximately yes.

Current position, operationally:

- M0 done
- M1 done baseline
- M2/M3 in progress
- M4 done
- M5 mostly established (gated baselines + CI checks)
- M6 started (actuator mailbox path)

That places us around the midpoint in risk-adjusted terms: the foundational execution model is real, but the long-run reliability and expanded peripheral contract still need disciplined closure.

## 7. Part II Preview

Part II begins where confidence gets expensive:

- parallel multi-board soaks
- baseline promotion under noisy USB realities
- final threshold tightening
- proving actuator path stability under sustained load

Part I was about building the cockpit while flying.
Part II is about flying instruments only.
