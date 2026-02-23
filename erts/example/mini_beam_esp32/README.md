# mini_beam_mcu

Minimal BEAM-like interpreter subset intended as a bring-up path for MCU-class
near-bare-metal environments with GPIO/PWM/I2C only.

## Scope (v0)

- Interpreter only (no JIT)
- Static bytecode image (no runtime loading)
- Integer registers only in this prototype
- BIF bridge for GPIO/PWM/I2C/time
- Mailbox primitive for command handoff

## Build (host simulation)

```bash
cmake -S erts/example/mini_beam_esp32 -B /tmp/mini_beam_esp32-build
cmake --build /tmp/mini_beam_esp32-build
/tmp/mini_beam_esp32-build/mini_beam_host
/tmp/mini_beam_esp32-build/mini_beam_host_mailbox
/tmp/mini_beam_esp32-build/mini_beam_host_regression
```

To prepare ESP-IDF HAL compilation path:

```bash
cmake -S erts/example/mini_beam_esp32 -B /tmp/mini_beam_esp32-build -DMB_HAL_BACKEND=espidf
```

## Zephyr/nRF52840 integration (primary)

A Zephyr wrapper app is available in `erts/example/mini_beam_esp32/zephyr_app`.

```bash
cd erts/example/mini_beam_esp32/zephyr_app
west build -b nrf52840dk/nrf52840
west flash
```

See `erts/example/mini_beam_esp32/zephyr_app/README.md` for board variants.
Board profile overlays are in `erts/example/mini_beam_esp32/zephyr_app/boards/`.

## ESP-IDF integration (legacy/optional)

An ESP-IDF wrapper app is available in `erts/example/mini_beam_esp32/espidf_app`.

```bash
cd erts/example/mini_beam_esp32/espidf_app
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

## Current files

- `include/mb_vm.h`: VM API and opcode/BIF enums
- `src/mb_vm.c`: bytecode interpreter and mailbox
- `include/mb_hal.h`: platform HAL contract
- `src/mb_hal_stub.c`: host stub HAL implementation
- `src/mb_hal_espidf.c`: ESP-IDF HAL implementation skeleton
- `src/mb_hal_nrf52.c`: Zephyr/nRF52 HAL implementation
- `src/main_host.c`: demo bytecode program
- `src/main_mailbox_host.c`: mailbox-driven control loop demo
- `src/main_regression_host.c`: host regression tests
- `espidf_app/main/app_main.c`: ESP-IDF app entry that runs the VM
- `zephyr_app/src/main.c`: Zephyr app entry that runs the VM
- `system/doc/mini_beam_esp32_contract_v1.md`: frozen v1 opcode/BIF ABI

## Next step toward nRF52840

Adapt pin mappings/devicetree aliases for your board and keep `mb_vm.c`
platform-neutral.
