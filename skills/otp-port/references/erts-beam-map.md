# ERTS/BEAM Source Map for Porting

Use this map to navigate runtime internals quickly during a port.

## Boot and Initialization

- Entry point: `erts/emulator/sys/unix/erl_main.c`
- Runtime startup sequence: `erts/emulator/beam/erl_init.c`
- Main thread handoff/platform loop: `erts/emulator/sys/unix/sys.c`

## Scheduler, Threads, and Synchronization

- Thread abstraction and atomics config template: `erts/include/internal/ethread_header_config.h.in`
- Scheduler and process runtime core: `erts/emulator/beam/erl_process.c`, `erts/emulator/beam/erl_process.h`
- Thread progress mechanism docs: `erts/emulator/internal_doc/ThreadProgress.md`

## VM Execution Core

- Interpreter ops and dispatch tables: `erts/emulator/beam/emu/*.tab`, `erts/emulator/beam/emu/beam_emu.c`
- JIT implementation: `erts/emulator/beam/jit/*`
- JIT design notes: `erts/emulator/internal_doc/BeamAsm.md`

## Memory and Allocation

- Allocator framework: `erts/emulator/beam/erl_alloc.c`, `erts/emulator/beam/erl_alloc_util.c`
- Virtual memory helpers: `erts/emulator/sys/common/erl_mmap.c`, `erts/emulator/sys/common/erl_mseg.c`
- Garbage collection internals: `erts/emulator/beam/erl_gc.c`, `erts/emulator/internal_doc/GarbageCollection.md`

## I/O, Ports, and Polling

- Poll backend abstraction: `erts/emulator/sys/common/erl_poll.c`
- Check I/O and event handling: `erts/emulator/sys/common/erl_check_io.c`
- Port runtime internals: `erts/emulator/beam/erl_port_task.c`, `erts/emulator/beam/erl_port.h`

## Build and Platform Selection

- Feature probes and OS/arch decisions: `erts/configure.ac`
- Emulator build flavor and source wiring: `erts/emulator/Makefile.in`
- Cross-compilation variables and examples: `xcomp/erl-xcomp.conf.template`, `xcomp/erl-xcomp-*.conf`, `HOWTO/INSTALL-CROSS.md`

## Embedded-Relevant Legacy/Signals

- Limited VxWorks artifact: `erts/emulator/drivers/vxworks/vxworks_resolv.c`
- Existing platform split is effectively `unix` vs `win32` via `ERLANG_OSTYPE` in `erts/configure.ac`.
- External comparative source: AtomVM project (`https://atomvm.org/`) for BEAM VM patterns on microcontrollers.
