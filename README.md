# OS/II

Write an Erlang term. Compile to bytecode. Flash to a $4 chip.
Two processes coordinate — sensor reads I2C, sends to actuator,
actuator drives PWM. If the sensor fails, the motor keeps running.
If the motor stalls, the sensor keeps reading. 42KB of RAM. No OS.
No malloc. No crash.

*BEAM-inspired embedded runtime for microcontrollers.*

```erlang
#{sensors => [
    #{bus => 1, addr => 16#39, reg => 16#92, poll_ms => 200}
  ],
  actuators => [
    #{kind => pwm, channel => 0}
  ],
  flows => [
    #{from => 16#39, to => {pwm, 0}}
  ],
  policy => #{
    mailbox_depth => 32,
    watchdog_ms => 6000,
    on_fail => stop_actuator
  }
}.
```

```
$ escript tools/flow_compile.escript flows/nano33_sensor_pwm.flow flow_generated.h
flow_compile: flows/nano33_sensor_pwm.flow -> flow_generated.h
  sensor 1: 73 bytes
  actuator: 17 bytes
  processes: 2
```

The compiler emits two bytecode programs.  The sensor process reads
I2C and SENDs to the actuator process, which calls PWM.  Both run
under a cooperative scheduler with per-process heaps and GC.

## Why

The nRF52840 runs at 64MHz with 256KB RAM.  That is roughly a 1994
Pentium in per-clock efficiency, minus the RAM.  BEAM's per-process
isolation (own registers, own heap, own mailbox) fits this constraint
naturally: each process is ~1.7KB, the scheduler is a round-robin scan,
and Cheney's copying GC runs without recursion on a small stack.

OS/II tests whether BEAM's model scales down to this level — and the
answer is yes.

## Quickstart

Requirements: Zephyr SDK, Arduino `bossac`, board on `/dev/ttyACM0`.

```bash
# bootstrap (first time)
./erts/example/mini_beam_esp32/zephyr_app/bootstrap_zephyr_workspace.sh ~/zephyrproject

# compile flow -> bytecode header
escript erts/example/mini_beam_esp32/tools/flow_compile.escript \
  erts/example/mini_beam_esp32/flows/nano33_sensor_pwm.flow \
  erts/example/mini_beam_esp32/zephyr_app/src/flow_generated.h

# build + flash + monitor
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_WS=~/zephyrproject
./erts/example/mini_beam_esp32/zephyr_app/reflash_nano33_sense.sh --monitor
```

Expected output:
```
i2c bus1 probe APDS9960 addr=0x39 reg=0x92 val=0xab
i2c bus1 probe HTS221 addr=0x5f reg=0x0f val=0xbc
flow: sensor pid=1 (73 bytes) actuator pid=2 (17 bytes)
event sensor=57:146 value=171 ts=3055 status=0 | actuator r0=2 r2=171
event sensor=57:146 value=171 ts=3255 status=0 | actuator r0=2 r2=171
```

## Host Tests (no hardware needed)

```bash
cd erts/example/mini_beam_esp32
mkdir -p build && cd build && cmake .. && make -j$(nproc)
./mini_beam_host_regression    # VM + scheduler + term/heap/GC tests
./mini_beam_host_multiproc     # two-process I2C-to-PWM proof
./mini_beam_host_stability     # 200K ticks, 100K messages, 2438 GC cycles
./mini_beam_host_flow          # flow-compiled bytecode verification
```

## Architecture

```
 .flow file (Erlang term)
       |
  flow_compile.escript        host: parse + validate + emit bytecode
       |
  flow_generated.h            C header with byte arrays
       |
  Zephyr firmware             boots, spawns processes via scheduler
       |
  +-----------+    SEND     +-----------+
  | sensor    | ----------> | actuator  |
  | process   |   mailbox   | process   |
  | (I2C BIF) |             | (PWM BIF) |
  +-----------+             +-----------+
       |                         |
  mb_scheduler_t             round-robin, 64 reductions/tick
       |
  mb_process_t               registers (tagged terms), heap, GC
       |
  mb_hal_nrf52.c             GPIO, PWM, I2C, monotonic time
```

## What's Inside

**VM** (`mb_vm.c`, 700 lines): register-based interpreter, 19 opcodes,
7 BIFs.  Registers are 32-bit tagged terms (4-bit tags, AtomVM-inspired).

**Scheduler** (`mb_scheduler.c`): cooperative round-robin, 8 process
slots, 64-reduction time slices.  Processes block on empty mailbox
(RECV_CMD) and yield on sleep (SLEEP_MS).

**Heap + GC** (`mb_heap.c`): per-process semi-space bump allocator.
Cheney's copying GC, no recursion.  128 words per space (512 bytes).

**Flow compiler** (`flow_compile.escript`): reads Erlang term, validates
sensor/actuator/flow/policy, emits C header with bytecode arrays.

**HAL** (`mb_hal_nrf52.c`): Zephyr devicetree bindings for GPIO, PWM,
I2C, monotonic time.

## Performance

Measured on Arduino Nano 33 BLE Sense (nRF52840 @ 64MHz):

| Metric | Baseline (200ms poll) | Stress (4 senders, no sleep) |
|--------|----------------------|------------------------------|
| Event rate | 5.0/s | 995/s (I2C saturated) |
| Period stdev | 0.8ms | n/a (tight loop) |
| Scheduler ticks/s | 1,223 | 4,023 |
| Mailbox peak depth | 0/32 | 0/32 |
| Errors | 0 | 0 |
| CPU utilization | <1% | 100% (I2C-bound) |
| Scheduler headroom | 99% | 97% |

RAM: 42KB / 256KB (16%).  Flash: 62KB / 928KB (7%).

Full report: `system/doc/M5_STRESS_TEST_REPORT.md`

## Roadmap

| Milestone | Status |
|-----------|--------|
| M0 Boot + execute | done |
| M1 Peripheral BIFs (GPIO, PWM, I2C) | done |
| M2 Process model + cooperative scheduler | done |
| M3 Tagged terms + bounded heaps + GC | done |
| M4 Watchdog + fault recovery | done |
| M5 Performance characterization | done |
| P0 Capability schema (boot-time) | done |
| P1 Profile + binding validation | done |
| P2 Flow compiler (.flow -> bytecode) | done |
| P3 Declarative policy engine | planned |
| P4 Multi-board portability proof | planned |

## Key Paths

```
erts/example/mini_beam_esp32/
  include/          VM headers (mb_vm.h, mb_process.h, mb_term.h, mb_heap.h, ...)
  src/              VM implementation + host test programs
  flows/            .flow files (Erlang terms)
  tools/            flow_compile.escript
  zephyr_app/       Zephyr firmware (main.c, prj.conf, overlays)

system/doc/
  mini_beam_esp32_contract_v1.md   opcode/BIF/ABI contract
  mini_beam_design_decisions.md    locked decisions + decision log
  M5_PERF_BASELINE_FLOW.md        performance baseline
  M5_STRESS_TEST_REPORT.md        stress test results
  OS2_CAPABILITY_DSL_PLAN.md       DSL track roadmap
```

## Non-Goals

- No distributed Erlang
- No hot code loading
- No dynamic NIF/driver loading
- No JIT

## License

Apache-2.0
