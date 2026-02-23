# ESP32 Porting Gap Analysis (ERTS/BEAM)

This file summarizes high-risk gaps for an ESP32 target and where they appear in source.

## 1. OS Model Mismatch

- Current build wiring sets `ERLANG_OSTYPE` to `unix` or `win32` (`erts/configure.ac`).
- ESP32/FreeRTOS is neither; introducing a new platform layer is mandatory.
- Primary patch surfaces: `erts/configure.ac`, `erts/emulator/Makefile.in`, new `erts/emulator/sys/<new_ostype>/` tree.

## 2. Heavy POSIX Dependencies

- `erts/emulator/sys/unix/sys.c` relies on signals, `sigaction`, `sigaltstack`, `pipe`, `select`, file descriptors, and `/proc`/`/sys` Linux paths.
- `erts/emulator/sys/common/erl_poll.c` expects poll/select/epoll/kqueue style FD semantics.
- A POSIX-compatibility shim or subsystem replacement is required.

## 3. Threading/Atomics Requirements

- Configure enforces thread library discovery and relies on `ethread` capabilities (`erts/configure.ac`, `erts/include/internal/ethread_header_config.h.in`).
- Scheduler model assumes strong threading primitives and memory ordering.
- FreeRTOS task + mutex + atomic coverage must satisfy ERTS assumptions before scheduler bring-up.

## 4. Memory Mapping Assumptions

- ERTS allocators and optional code management rely on virtual memory helpers (`erl_mmap.c`, `erl_mseg.c`).
- ESP32 memory model is constrained; some mmap/mseg paths may require stubs or alternate allocators.

## 5. JIT and Codegen Constraints

- JIT is only supported on x86-64/aarch64 (`erts/configure.ac`, `erts/emulator/internal_doc/BeamAsm.md`).
- ESP32 target must run in interpreter mode (`emu`) initially.
- Disable JIT explicitly during configuration.

## 6. Dynamic Loading and Drivers

- Embedded environments often lack full `dlopen` behavior expected by dynamic drivers/NIFs.
- Prefer static linking profile and disable or defer dynamic driver/NIF workflows.

## 7. Practical Staging Recommendation

1. Build smallest possible ERTS profile with JIT disabled and minimal apps.
2. Bring up scheduler/time/memory primitives on target OS abstraction.
3. Boot to minimal Erlang init path (`kernel`, `stdlib`) without full networking.
4. Add I/O poll and port capabilities incrementally after stable process scheduling.

## Suggested Initial Configure Direction

- Start from cross-compilation flow in `HOWTO/INSTALL-CROSS.md`.
- Use `xcomp/erl-xcomp.conf.template` as baseline and provide explicit values for cross-test variables.
- Disable or avoid optional facilities that are known mismatches (JIT, advanced kernel poll, dynamic loading where unavailable).
- Compare subsystem scope and feature cuts against AtomVM (`https://atomvm.org/`) to calibrate a realistic first embedded milestone.

This is a source-first initial assessment, not proof of feasibility for a full OTP feature set on ESP32.
