# OS/II Board Debug Guide (nRF52840)

This guide is for debugging the OS/II mini VM on real hardware.

## Supported Boards

- `nrf52840dk/nrf52840` (recommended first)
- `arduino_nano_33_ble`

## Prerequisites

1. Zephyr/NCS toolchain installed (`west`, `cmake`, `ninja`).
2. J-Link access (nRF DK has onboard debugger).
3. USB serial access for logs.

## Quick Start (nRF52840DK)

```bash
cd erts/example/mini_beam_esp32/zephyr_app
west build -b nrf52840dk/nrf52840 -p always
west flash
```

Open logs (one option):

```bash
west debug
```

If you use serial monitor directly (Linux):

```bash
ls /dev/ttyACM*
screen /dev/ttyACM0 115200
```

Expected log markers:
- `mini_beam_nrf52 start`
- `vm done i2c_value=...` or explicit error with `vm failed`

## Quick Start (Arduino Nano 33 BLE)

```bash
cd erts/example/mini_beam_esp32/zephyr_app
west build -b arduino_nano_33_ble -p always
west flash
```

Note: flashing/serial device names may differ from DK and can require
bootloader reset timing.

### Nano 33 BLE Flash Loop Escape (SAM-BA/BOSSA)

If you see repeated errors like:
- `No device found on /dev/ttyACM0`
- `SAM-BA operation failed`

use this deterministic sequence:

1. Use Arduino `bossac` (required for this board class):
```bash
export BOSSAC=/home/io/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac
$BOSSAC --help | head -n 2
```
2. Build once:
```bash
west build -b arduino_nano_33_ble -d /tmp/os2-nano33-build -p always
```
3. Put board in bootloader mode (double-tap reset button). LED should pulse.
4. Immediately flash with explicit runner binary:
```bash
west flash -d /tmp/os2-nano33-build --runner bossac \
  --bossac=$BOSSAC --bossac-port /dev/ttyACM0
```
5. If upload still fails, probe both ports because bootloader/runtime can
   re-enumerate differently:
```bash
ls -l /dev/ttyACM*
$BOSSAC -p /dev/ttyACM0 -i || true
$BOSSAC -p /dev/ttyACM1 -i || true
```

If the board does not stay in bootloader long enough over USB, stop looping and
switch to SWD flashing/debug (next section).

### Serial Ownership Across Reset/Re-enumeration (Linux)

Nano 33 BLE/Sense re-enumerates USB during reset/flash. The recreated
`/dev/ttyACM0` can come back with different ownership and block tools.

Background:
- `/dev/ttyACM*` nodes are created by kernel `devtmpfs` (typically root-owned).
- `udev` rules then apply final owner/group/mode.
- So the persistent fix is a local `udev` rule in `/etc/udev/rules.d/`.

Quick workaround (per reconnect/reset):
```bash
sudo chown io:io /dev/ttyACM0
```

Recommended persistent fix (udev rule):
```bash
sudo tee /etc/udev/rules.d/99-nano33-tty.rules >/dev/null <<'EOF'
# Option A (single-user dev machine): force io:io ownership
SUBSYSTEM=="tty", KERNEL=="ttyACM*", ATTRS{idVendor}=="2341", ATTRS{idProduct}=="005a", OWNER="io", GROUP="io", MODE="0660", SYMLINK+="nano33"
SUBSYSTEM=="tty", KERNEL=="ttyACM*", ATTRS{idVendor}=="2fe3", ATTRS{idProduct}=="0100", OWNER="io", GROUP="io", MODE="0660", SYMLINK+="nano33"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=tty
```

Multi-user alternative (recommended if multiple users share this host):
```bash
sudo tee /etc/udev/rules.d/99-nano33-tty.rules >/dev/null <<'EOF'
# Option B (shared machine): use dialout ACL model
SUBSYSTEM=="tty", KERNEL=="ttyACM*", ATTRS{idVendor}=="2341", ATTRS{idProduct}=="005a", GROUP="dialout", MODE="0660", TAG+="uaccess", SYMLINK+="nano33"
SUBSYSTEM=="tty", KERNEL=="ttyACM*", ATTRS{idVendor}=="2fe3", ATTRS{idProduct}=="0100", GROUP="dialout", MODE="0660", TAG+="uaccess", SYMLINK+="nano33"
EOF
```

Verify after reconnect:
```bash
ls -l /dev/ttyACM0 /dev/nano33
id -nG
udevadm info -a -n /dev/ttyACM0 | rg "idVendor|idProduct" -n
```

### SWD Fallback (Recommended when USB flashing is unstable)

- Use an external SWD probe (or nRF DK as probe) to flash/debug the nRF52840
  directly.
- Keep Nano 33 BLE as runtime target and verify OS/II behavior over UART/RTT.
- This path avoids SAM-BA timing issues and is the fastest way to keep M1/M2
  milestones moving.

## Board Profile and Alias Rules

OS/II uses logical channels mapped by devicetree aliases:

- GPIO channel `0` -> `led0`
- PWM channel `0` -> `pwm0` node label
- I2C bus `0` -> `i2c0`

Overlay files:
- `boards/nrf52840dk_nrf52840.overlay`
- `boards/arduino_nano_33_ble.overlay`

Build runs with strict checks (`MB_STRICT_DTS`), so missing aliases fail at compile time.

## Debug Workflow

1. Build clean:
```bash
west build -b nrf52840dk/nrf52840 -p always
```
2. Flash:
```bash
west flash
```
3. Capture logs.
4. If failure, collect:
- full build error output
- first 100 lines of runtime log
- board name and connected peripherals
- `dmesg -w` output during reset/flash attempt

## Peripheral Bring-Up Order

1. GPIO only (confirm LED toggle path).
2. PWM output (confirm duty/frequency with scope).
3. I2C read from known sensor register.
4. Then expand to SAADC/UARTE/SPIM/QDEC stages.

## Common Issues

1. Missing alias compile errors
- Cause: overlay mismatch.
- Fix: update board overlay/DTS to provide `led0`, `i2c0`, and enabled `pwm0`.

2. I2C read returns error
- Cause: wrong bus pins, address, or no pull-ups.
- Fix: verify wiring, sensor address, and pull-up resistors.

3. PWM appears inactive
- Cause: pin not routed by board DTS or wrong channel.
- Fix: inspect board DTS and overlay pin assignment.

4. No logs on serial
- Cause: wrong TTY device or monitor speed.
- Fix: verify `/dev/ttyACM*`, re-open monitor at `115200`.

5. Permission denied / tty busy after reset
- Cause: device re-enumeration recreates `/dev/ttyACM0` ownership/ACL.
- Fix: apply temporary `sudo chown io:io /dev/ttyACM0` or install persistent
  udev rule (section above).

## Useful Commands

Rebuild from scratch:
```bash
west build -t pristine
west build -b nrf52840dk/nrf52840
```

List generated devicetree:
```bash
west build -t menuconfig
# and inspect build/zephyr/zephyr.dts
```

Reliable serial logger (auto-reconnect, file append):
```bash
./start_serial_log.sh --log logs/nano33.log --sudo-chown
tail -n +1 -F logs/nano33.log --sleep-interval=0.1
```

One-command rebuild + redeploy + logging:
```bash
./redeploy_and_log.sh --log logs/nano33.log --sudo-chown
```

## What to Share for Fast Debug Help

- board target (`nrf52840dk/nrf52840` or `arduino_nano_33_ble`)
- command used
- full error output
- first runtime logs
- connected sensor/peripheral details
