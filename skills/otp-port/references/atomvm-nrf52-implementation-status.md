# AtomVM nRF52 Implementation Status (Draft)

This note records what is already implemented in the local AtomVM nRF52 stream and what remains open.

Primary working tree:
- `/home/io/projects/learn_erl/otp/atomvm_nrf52840`
- branch: `feature/nrf52840-port-p0`

## Implemented

1. nRF52 platform scaffold
- `src/platforms/nrf52/CMakeLists.txt`
- smoke targets:
  - `atomvm_nrf52_smoke`
  - `atomvm_nrf52_runtime_smoke`
- options:
  - `AVM_NRF52_SMOKE_ONLY`
  - `AVM_NRF52_STANDALONE_RUNTIME`
  - `AVM_NRF52_ENABLE_FULL_RUNTIME`
  - `AVM_NRF52_ENABLE_STARTUP_MODULE`

2. Runtime/source wiring
- `src/platforms/nrf52/src/CMakeLists.txt`
- `src/platforms/nrf52/src/main.c`
- `src/platforms/nrf52/src/lib/CMakeLists.txt`
- runtime libs:
  - `atomvm_nrf52_runtime_lib_stub`
  - `atomvm_nrf52_runtime_lib_core`
- object stubs target:
  - `atomvm_nrf52_platform_stubs`

3. First-pass platform hooks and nifs/defaultatoms
- `src/platforms/nrf52/src/lib/sys.c`
- `src/platforms/nrf52/src/lib/nrf52_sys.h`
- `src/platforms/nrf52/src/lib/platform_defaultatoms.{h,c}`
- `src/platforms/nrf52/src/lib/platform_nifs.c`

4. Top-level platform selection (draft)
- root `CMakeLists.txt` supports:
  - `AVM_PLATFORM=auto|generic_unix|nrf52`
- `nrf52` path is explicitly draft and runtime-only constrained.

## Validation scripts

- `src/platforms/nrf52/tests/compile_nrf52_scaffold.sh`
- `src/platforms/nrf52/tests/smoke_uart_banner.sh`
- `src/platforms/nrf52/tests/runtime_boot_smoke.sh`
- `src/platforms/nrf52/tests/pr_slice_p1_checks.sh`
- `src/platforms/nrf52/tests/ci_nrf52_p1.sh`
- `src/platforms/nrf52/tests/full_runtime_gate_check.sh`
- `src/platforms/nrf52/tests/next_block_suite.sh`
- `src/platforms/nrf52/tests/top_level_platform_select_check.sh`

## Known gaps

1. Full runtime integration is still gated
- `AVM_NRF52_ENABLE_FULL_RUNTIME=ON` intentionally fails unless a proper `libAtomVM` platform flow is available from top-level selection.

2. Startup module execution is not wired
- Non-stub path logs TODO for startup module loading (packbeam source unresolved).

3. Toolchain/board flow not finalized
- nRF52 selector is host-side draft, not a completed board toolchain integration like rp2/stm32.

## Operator commands

1. Local nRF52 suite (platform subtree)
```bash
cd /home/io/projects/learn_erl/otp/atomvm_nrf52840
./src/platforms/nrf52/tests/next_block_suite.sh
```

2. Top-level selector draft check
```bash
cd /home/io/projects/learn_erl/otp/atomvm_nrf52840
./src/platforms/nrf52/tests/top_level_platform_select_check.sh
```

## Decision summary

- Keep current implementation as a draft scaffold and validation harness.
- Next engineering objective: replace draft selector behavior with full nRF52 toolchain/board integration and real runtime startup path.
