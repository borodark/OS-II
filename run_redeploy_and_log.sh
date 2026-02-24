#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$REPO_ROOT/erts/example/mini_beam_esp32/zephyr_app"

ZEPHYR_WS="${ZEPHYR_WS:-/home/io/zephyrproject}"
BOSSAC="${BOSSAC:-$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac}"
LOG_FILE="${LOG_FILE:-$APP_DIR/logs/nano33.log}"

if [[ -f "$ZEPHYR_WS/.venv/bin/activate" ]]; then
  # shellcheck disable=SC1090
  source "$ZEPHYR_WS/.venv/bin/activate"
fi

export ZEPHYR_WS
export BOSSAC

cd "$APP_DIR"
exec ./redeploy_and_log.sh --log "$LOG_FILE" --sudo-chown "$@"
