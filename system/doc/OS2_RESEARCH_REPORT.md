# OS/II: A BEAM-Inspired Runtime for Microcontrollers

## Abstract

OS/II is a research runtime that adapts the BEAM virtual machine's
process isolation model to nRF52840-class microcontrollers (64MHz
Cortex-M4, 256KB RAM).  The runtime implements cooperative multi-process
scheduling, tagged-term registers, per-process heaps with copying garbage
collection, and a flow compiler that translates Erlang-term
specifications into bytecode.  On hardware, OS/II sustains 995 real I2C
sensor events per second across four concurrent processes with zero
mailbox drops, using less than 3% of available CPU for scheduling
overhead.  The entire runtime — VM, scheduler, heap, GC, and HAL —
occupies 42KB of RAM and 62KB of flash.

## 1. Motivation

The nRF52840 SoC provides 64MHz of Cortex-M4 computation with 256KB
of SRAM.  In per-clock efficiency, this is comparable to a mid-1990s
desktop processor, but with two orders of magnitude less memory.
Traditional embedded development on such hardware uses C with manual
state machines and interrupt-driven I/O — an approach that scales
poorly as sensor and actuator counts grow.

The BEAM virtual machine, which underlies Erlang and Elixir, solves
coordination problems through lightweight processes with isolated heaps,
bounded mailboxes, and preemptive scheduling.  These properties align
well with embedded constraints: isolation prevents memory corruption
between subsystems, bounded mailboxes provide backpressure, and
per-process GC eliminates global pause times.

The question OS/II investigates: can BEAM's process model scale
**down** to a 256KB microcontroller while remaining practical for
real-time sensor/actuator orchestration?

## 2. Architecture

OS/II consists of five layers:

**Register VM** (`mb_vm.c`).  A bytecode interpreter operating on
16 tagged 32-bit registers.  19 opcodes cover arithmetic, control flow,
BIF dispatch, inter-process messaging (SEND, RECV_CMD), and heap
allocation (MAKE_TUPLE, CONS).  The instruction set is intentionally
minimal — complex operations are expressed through BIF calls into the
native HAL layer.

**Process Model** (`mb_process.h`, `mb_scheduler.c`).  Each process
owns its register file, program counter, mailbox (32-slot circular
queue), and heap.  A process is approximately 1,776 bytes.  The
scheduler maintains a fixed table of 8 process slots and dispatches
in round-robin order with 64-reduction time slices.  Processes yield
cooperatively: RECV_CMD on an empty mailbox transitions to WAITING
(woken on message arrival), SLEEP_MS transitions to SLEEPING (woken
by wall-clock deadline), and YIELD exhausts the reduction budget.

**Tagged Terms** (`mb_term.h`).  Registers hold `uint32_t` values with
4-bit type tags, following AtomVM's encoding scheme:

| Tag | Type | Payload |
|-----|------|---------|
| 0xF | Small integer | 28-bit signed (-134M to +134M) |
| 0xB | Atom | 28-bit index (nil=0, true=1, false=2) |
| 0x3 | PID | 28-bit process ID |
| 0x2 | Boxed pointer | 28-bit word offset (tuple) |
| 0x1 | Cons pointer | 28-bit word offset (list cell) |

The tagging boundary sits at the mailbox interface: RECV_CMD tags
incoming `int32_t` command fields as small integers; SEND untags
register values back to raw integers for the command ABI.  This
preserves the external mailbox format while giving the VM type safety
internally.

**Heap and GC** (`mb_heap.c`).  Each process has a semi-space heap:
two regions of 128 words (512 bytes each).  Allocation is a bump
pointer.  When allocation fails, Cheney's copying collector runs:
swap spaces, copy roots (all 16 registers), BFS-scan copied objects
to transitively copy children, update forwarding pointers.  The
algorithm uses no recursion — critical for the 8KB Zephyr main stack.
Collection is per-process, so only one process pauses at a time,
exactly as in BEAM.

**Flow Compiler** (`flow_compile.escript`).  An Erlang escript that
reads a `.flow` file — a standard Erlang term — and emits a C header
containing bytecode arrays.  The compiler validates sensor addresses,
actuator channels, and flow connectivity against the declared schema.
For each sensor in the flow, it generates a process program (I2C read,
SEND to actuator, sleep); for actuators, a receiver program (RECV_CMD,
PWM BIF call).  The generated header is included at compile time — no
runtime parsing on the MCU.

## 3. Hardware Validation

All measurements on Arduino Nano 33 BLE Sense (nRF52840, 64MHz,
256KB RAM) running Zephyr RTOS 3.7.0.

### 3.1 Sensor Discovery

At boot, the runtime probes 14 I2C sensor signatures with a
timeout-guarded thread (500ms deadline per probe).  On the test board:
APDS9960 (light/gesture, addr 0x39) and HTS221 (temperature/humidity,
addr 0x5F) are detected.

### 3.2 Two-Process Flow

A flow-compiled program spawns two processes:

- **Sensor process** (pid 1, 73 bytes bytecode): reads APDS9960 via
  I2C BIF every 200ms, SENDs value as PWM_SET_DUTY command to
  actuator.
- **Actuator process** (pid 2, 17 bytes bytecode): blocks on RECV_CMD,
  calls PWM BIF with received duty value.

Observed output (real hardware, real sensor data):
```
event sensor=57:146 value=171 ts=3055 status=0 | actuator r0=2 r2=171
```

The sensor reads 171 (0xAB, the APDS9960 device ID register).  The
actuator receives PWM_SET_DUTY with duty=171.  End-to-end flow
operates at 202.5ms mean period with 0.8ms standard deviation.

### 3.3 Stress Test

Four sensor processes in tight loops (YIELD, no sleep) sending to one
actuator:

| Metric | Value |
|--------|-------|
| Scheduler ticks/s | 4,023 |
| I2C events/s | 995 |
| VM instructions/s | ~257,000 |
| Mailbox peak depth | 0/32 |
| Errors | 0 |
| CPU utilization | 100% (I2C-bound) |

The bottleneck is the I2C bus at 400kHz.  The scheduler consumes less
than 3% of CPU at saturation — measured by comparing the 4,023
active ticks/s against the 120,000 idle ticks/s observed when all
processes are sleeping.

### 3.4 Stability

Host-side long-run test: 200,000 scheduler ticks, two processes
exchanging 100,000 messages with continuous heap allocation.  2,438 GC
cycles completed.  Heap pointer stays bounded, no memory corruption,
no errors.

### 3.5 Resource Usage

| Resource | Used | Available | Utilization |
|----------|------|-----------|-------------|
| RAM | 42 KB | 256 KB | 16.4% |
| Flash | 62 KB | 928 KB | 6.7% |
| Processes | 5 (stress) | 8 (max) | 62.5% |
| Mailbox | 0/32 peak | 32 slots | 0% |

## 4. Design Decisions

### 4.1 Cooperative vs. Preemptive Scheduling

OS/II uses cooperative scheduling with reduction counting.  On
Cortex-M4, preemptive context switching requires saving/restoring the
full register set via PendSV, and all data structures accessed by the
VM must be interrupt-safe.  Cooperative scheduling avoids both costs.
The risk — a misbehaving process starving others — is mitigated by the
reduction budget (64 instructions per tick) and the fact that all
programs are compiler-generated, not user-written.

### 4.2 Fixed Process Table vs. Dynamic Allocation

The process table is a fixed array of 8 slots.  Dynamic allocation
(malloc/free) on a 256KB MCU introduces fragmentation risk and
requires a heap allocator that the Zephyr kernel may not provide in
minimal configurations.  Eight slots are sufficient for sensor/actuator
flows; if more are needed, the constant is a compile-time define.

### 4.3 Erlang Terms as Flow Format

The flow file format is an Erlang term parsed by `file:consult/1`.
Alternatives considered: custom key-value format (insufficient for
nested structures), INI sections (ambiguous cross-references), JSON
(parser dependency, ecosystem mismatch).  Erlang terms are native to
the project's ecosystem, free to parse with standard tooling, and
unambiguous for AI-assisted generation — an engineer can dictate a
flow specification verbally and a language model can produce a valid
`.flow` file.

### 4.4 Tagging Boundary at the Mailbox

The mailbox command ABI uses raw `int32_t` fields.  Tags are applied
at RECV_CMD and stripped at SEND.  This means native C code can push
commands into a process's mailbox without knowing about tagged terms,
and the I2C/PWM HAL functions receive plain integers.  The alternative
— tagged terms in the mailbox — would require updating all native
enqueue call sites and HAL functions, with no benefit to safety since
the mailbox already validates command structure on push.

### 4.5 No Pure-Compute Benchmark

The stress test stops at I2C bus saturation (995 events/s).  A
synthetic benchmark removing I2C would measure the VM in isolation,
but the nRF52840 is a sensor/actuator platform.  Every deployable
workload involves peripheral I/O.  The scheduler's 97% headroom under
full I2C load demonstrates that the runtime disappears behind the
hardware — which is the design goal.

## 5. Related Work

**AtomVM** runs a full BEAM bytecode interpreter on ESP32 and
nRF52-class MCUs.  It supports OTP modules, pattern matching, and
the standard library.  OS/II is deliberately smaller: 19 opcodes vs.
BEAM's ~160, no module loader, no pattern matching, no OTP.  The
tradeoff is that OS/II's flow compiler generates purpose-built bytecode
for sensor/actuator loops, avoiding the overhead of a general-purpose
BEAM runtime.

**Nerves** and **GRiSP** run BEAM on Linux-capable boards
(Raspberry Pi, GRiSP-2) with full OTP supervision trees.  They target
a different class of hardware — boards with 512MB+ RAM and MMU — and
solve different problems (firmware updates, fault tolerance at the
application level).  OS/II operates below this level, on bare-metal
MCUs without an OS kernel (Zephyr provides the RTOS substrate).

**Tock OS** uses a capability-based architecture with Rust capsules
for driver isolation on Cortex-M.  OS/II's process isolation serves a
similar purpose but uses BEAM's message-passing model instead of
Rust's type system.  Tock does not include a bytecode VM or flow
compiler.

## 6. Limitations and Future Work

**No preemption.**  A BIF call (e.g., I2C read) blocks the scheduler
for the duration of the peripheral transaction.  If a transaction
hangs, all processes stall.  The I2C probe timeout (dedicated thread)
is a workaround, not a solution.  True preemption would require
PendSV-based context switching.

**No dynamic process spawning.**  Processes are spawned at boot from
compiled flow files.  Runtime spawn (e.g., for on-demand tasks) would
require extending the SEND/RECV protocol and adding a spawn opcode.

**Single-board validation.**  OS/II has been tested on two Arduino
Nano 33 BLE Sense boards, both nRF52840.  The P4 milestone
(multi-board portability) would validate the same flow file on ESP32
or a different nRF52 variant.

**No pattern matching.**  The VM has no match/case opcode.  Flow
decisions (sensor value thresholds, filtering) must be compiled into
explicit comparison sequences.  Adding a MATCH opcode with guard
support would enable more expressive flows.

**Policy engine (P3).**  The flow file declares `on_fail =>
stop_actuator` but the runtime does not yet enforce it.  Implementing
declarative policies requires extending the actuator process with
a fallback path and a fault-state register.

## 7. Conclusion

BEAM's process model scales to a 256KB microcontroller.  The critical
insight is that isolation, message passing, and per-process GC are
structural properties — they depend on data layout, not on compute
power.  A process is 1.7KB.  A GC cycle copies live data without
recursion.  A scheduler tick scans a fixed array.  None of these
operations require MHz or megabytes.

What does require resources is generality.  OS/II trades generality
for fit: no modules, no pattern matching, no dynamic code loading.
In return, the runtime is invisible behind the I2C bus — the hardware
is the bottleneck, not the software.

For sensor/actuator orchestration on constrained MCUs, this is the
right tradeoff.
