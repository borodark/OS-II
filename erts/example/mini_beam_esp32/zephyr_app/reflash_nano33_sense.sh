#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="${APP_DIR:-$SCRIPT_DIR}"
BUILD_DIR="${BUILD_DIR:-/tmp/os2-nano33-sense-build}"
BOARD="${BOARD:-arduino_nano_33_ble/nrf52840/sense}"
PORT="${PORT:-/dev/ttyACM0}"
BOSSAC="${BOSSAC:-$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac}"
ZEPHYR_WS="${ZEPHYR_WS:-${WEST_TOPDIR:-}}"

MONITOR=0
if [[ "${1:-}" == "--monitor" ]]; then
  MONITOR=1
fi

if [[ -z "${ZEPHYR_WS}" ]]; then
  if command -v west >/dev/null 2>&1; then
    ZEPHYR_WS="$(west topdir 2>/dev/null || true)"
  fi
fi

if [[ -z "${ZEPHYR_WS}" ]]; then
  echo "error: ZEPHYR_WS is not set and no west workspace was detected." >&2
  echo "set ZEPHYR_WS=/path/to/zephyr-workspace and retry." >&2
  exit 1
fi

if [[ ! -x "$BOSSAC" ]]; then
  echo "error: bossac not found at: $BOSSAC" >&2
  echo "set BOSSAC=/path/to/arduino-bossac and retry" >&2
  exit 1
fi

if [[ -f "$ZEPHYR_WS/.venv/bin/activate" ]]; then
  # shellcheck disable=SC1090
  source "$ZEPHYR_WS/.venv/bin/activate"
fi

if ! command -v west >/dev/null 2>&1; then
  echo "error: west not found in PATH. Activate your Zephyr venv first." >&2
  exit 1
fi

cd "$ZEPHYR_WS"

export XDG_CACHE_HOME="${XDG_CACHE_HOME:-/tmp/zephyr-cache}"
export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-gnuarmemb}"
export GNUARMEMB_TOOLCHAIN_PATH="${GNUARMEMB_TOOLCHAIN_PATH:-/usr}"

sudo chown io:io /dev/ttyACM0

echo "[1/2] Building $BOARD into $BUILD_DIR"
west build -b "$BOARD" -s "$APP_DIR" -d "$BUILD_DIR" -p always

echo "[2/2] Flashing via bossac on $PORT"
west flash -d "$BUILD_DIR" --runner bossac --bossac="$BOSSAC" --bossac-port "$PORT"

sudo chown io:io /dev/ttyACM0

echo "Flash complete."
if [[ "$MONITOR" -eq 1 ]]; then
  echo "Opening serial monitor on $PORT @115200 (Ctrl-A then K to quit screen)"
  exec screen "$PORT" 115200
fi
