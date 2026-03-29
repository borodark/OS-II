#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="${APP_DIR:-$SCRIPT_DIR}"
BUILD_DIR="${BUILD_DIR:-/tmp/os2-nano33-sense-build}"
BOARD="${BOARD:-arduino_nano_33_ble/nrf52840/sense}"
PORT="${PORT:-}"
ARDUINO_BOSSAC_DEFAULT="$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac"
BOSSAC="${BOSSAC:-$ARDUINO_BOSSAC_DEFAULT}"
ZEPHYR_WS="${ZEPHYR_WS:-${WEST_TOPDIR:-}}"
SUDO_CHOWN="${SUDO_CHOWN:-0}"
WAIT_BOOT_SECS="${WAIT_BOOT_SECS:-3}"
OS2_FAULT_EVERY_N="${OS2_FAULT_EVERY_N:-}"
OS2_ENABLE_TASK_WDT="${OS2_ENABLE_TASK_WDT:-}"
FLASH_ATTEMPTS="${FLASH_ATTEMPTS:-3}"

MONITOR=0
if [[ "${1:-}" == "--monitor" ]]; then
  MONITOR=1
fi

autodetect_port() {
  local candidate
  if [[ -n "${PORT:-}" && -e "$PORT" ]]; then
    return 0
  fi
  if [[ -e /dev/ttyACM0 ]]; then
    PORT="/dev/ttyACM0"
    return 0
  fi
  candidate="$(ls /dev/ttyACM* 2>/dev/null | head -n1 || true)"
  if [[ -n "$candidate" ]]; then
    PORT="$candidate"
    return 0
  fi
  PORT="/dev/ttyACM0"
  return 1
}

autodetect_bossac() {
  if [[ -n "${BOSSAC:-}" && -x "$BOSSAC" ]]; then
    return 0
  fi
  if [[ -x "$ARDUINO_BOSSAC_DEFAULT" ]]; then
    BOSSAC="$ARDUINO_BOSSAC_DEFAULT"
    return 0
  fi
  if command -v bossac >/dev/null 2>&1; then
    BOSSAC="$(command -v bossac)"
    return 0
  fi
  return 1
}

if [[ -z "${ZEPHYR_WS}" ]]; then
  if command -v west >/dev/null 2>&1; then
    ZEPHYR_WS="$(west topdir 2>/dev/null || true)"
  fi
fi

if [[ -z "${ZEPHYR_WS}" ]]; then
  if [[ -d "$HOME/zephyrproject/.west" ]]; then
    ZEPHYR_WS="$HOME/zephyrproject"
  fi
fi

if [[ -z "${ZEPHYR_WS}" ]]; then
  echo "error: Zephyr workspace not found." >&2
  echo "tried: ZEPHYR_WS env, west topdir, \$HOME/zephyrproject" >&2
  exit 1
fi

if [[ -f "$ZEPHYR_WS/.venv/bin/activate" ]]; then
  # shellcheck disable=SC1090
  source "$ZEPHYR_WS/.venv/bin/activate"
fi

if ! command -v west >/dev/null 2>&1; then
  if [[ -x "$ZEPHYR_WS/.venv/bin/west" ]]; then
    export PATH="$ZEPHYR_WS/.venv/bin:$PATH"
  fi
fi

if ! command -v west >/dev/null 2>&1; then
  echo "error: west not found. expected at $ZEPHYR_WS/.venv/bin/west or in PATH." >&2
  exit 1
fi

if ! autodetect_bossac; then
  echo "error: bossac not found. install it or set BOSSAC=/path/to/bossac" >&2
  exit 1
fi

autodetect_port || true
if [[ -z "${PORT:-}" ]]; then
  echo "error: no serial port detected (expected /dev/ttyACM*)." >&2
  exit 1
fi

cd "$ZEPHYR_WS"

export XDG_CACHE_HOME="${XDG_CACHE_HOME:-/tmp/zephyr-cache}"
export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-gnuarmemb}"
export GNUARMEMB_TOOLCHAIN_PATH="${GNUARMEMB_TOOLCHAIN_PATH:-/usr}"
export CCACHE_DISABLE="${CCACHE_DISABLE:-1}"

ensure_port_owner() {
  if [[ "$SUDO_CHOWN" -eq 1 && -e "$PORT" ]]; then
    sudo chown "$USER:$USER" "$PORT" >/dev/null 2>&1 || true
  fi
}

wait_for_port() {
  local wait_secs="${1:-8}"
  local i
  for i in $(seq 1 "$wait_secs"); do
    if [[ -e "$PORT" ]]; then
      return 0
    fi
    sleep 1
  done
  return 1
}

touch_bootloader_1200() {
  # On Nano33 this usually triggers bootloader re-enumeration.
  if [[ -e "$PORT" ]]; then
    stty -F "$PORT" 1200 hupcl >/dev/null 2>&1 || true
  fi
}

echo "[env] ZEPHYR_WS=$ZEPHYR_WS"
echo "[env] BOSSAC=$BOSSAC"
echo "[env] PORT=$PORT"

echo "[1/2] Building $BOARD into $BUILD_DIR"
BUILD_ARGS=(
  -b "$BOARD"
  -s "$APP_DIR"
  -d "$BUILD_DIR"
  -p always
)
CMAKE_DEFINES=()
EXTRA_CPP_FLAGS=()
if [[ -n "$OS2_FAULT_EVERY_N" ]]; then
  echo "[build] fault injection enabled: OS2_FAULT_EVERY_N=$OS2_FAULT_EVERY_N"
  EXTRA_CPP_FLAGS+=("-DOS2_FAULT_EVERY_N=$OS2_FAULT_EVERY_N")
fi
if [[ -n "$OS2_ENABLE_TASK_WDT" ]]; then
  echo "[build] task watchdog flag: OS2_ENABLE_TASK_WDT=$OS2_ENABLE_TASK_WDT"
  EXTRA_CPP_FLAGS+=("-DOS2_ENABLE_TASK_WDT=$OS2_ENABLE_TASK_WDT")
fi
if [[ "${#EXTRA_CPP_FLAGS[@]}" -gt 0 ]]; then
  extra_cflags="${EXTRA_CPP_FLAGS[*]}"
  echo "[build] extra cflags: $extra_cflags"
  CMAKE_DEFINES+=("-DEXTRA_CFLAGS=$extra_cflags")
fi
if [[ "${#CMAKE_DEFINES[@]}" -gt 0 ]]; then
  BUILD_ARGS+=(-- "${CMAKE_DEFINES[@]}")
fi
west build "${BUILD_ARGS[@]}"

echo "[pre-flash] Releasing serial monitor processes on $PORT (if any)"
pkill -f "screen $PORT" 2>/dev/null || true
pkill -f "picocom.*$PORT" 2>/dev/null || true
ensure_port_owner

echo "[pre-flash] Trigger bootloader via 1200-baud touch"
touch_bootloader_1200
sleep "$WAIT_BOOT_SECS"
wait_for_port "${WAIT_BOOT_SECS}" || true
ensure_port_owner

echo "[2/2] Flashing via bossac on $PORT"
FLASH_OK=0
for attempt in $(seq 1 "$FLASH_ATTEMPTS"); do
  echo "[flash] attempt ${attempt}/${FLASH_ATTEMPTS}"
  if west flash -d "$BUILD_DIR" --runner bossac --bossac="$BOSSAC" --bossac-port "$PORT"; then
    FLASH_OK=1
    break
  fi
  echo "[flash] attempt ${attempt} failed; re-triggering bootloader touch"
  touch_bootloader_1200
  sleep "$WAIT_BOOT_SECS"
  wait_for_port "${WAIT_BOOT_SECS}" || true
  ensure_port_owner
done

if [[ "$FLASH_OK" -ne 1 ]]; then
  echo "error: flashing failed after ${FLASH_ATTEMPTS} attempts" >&2
  exit 1
fi

wait_for_port 8 || true
ensure_port_owner

echo "Flash complete."
if [[ "$MONITOR" -eq 1 ]]; then
  echo "Opening serial monitor on $PORT @115200 (Ctrl-A then K to quit screen)"
  exec screen "$PORT" 115200
fi
