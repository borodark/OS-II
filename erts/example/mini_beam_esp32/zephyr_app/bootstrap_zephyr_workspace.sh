#!/usr/bin/env bash
set -euo pipefail

# Bootstraps a Zephyr v3.7 workspace with a local Python venv and west.
# Usage:
#   ./bootstrap_zephyr_workspace.sh [workspace_dir]
#
# Example:
#   ./bootstrap_zephyr_workspace.sh "$HOME/zephyrproject"

WS="${1:-$HOME/zephyrproject}"
ZEPHYR_TAG="${ZEPHYR_TAG:-v3.7.0}"

echo "[1/6] Prepare workspace: $WS"
mkdir -p "$WS"

echo "[2/6] Create/activate Python venv"
python3 -m venv "$WS/.venv"
# shellcheck disable=SC1090
source "$WS/.venv/bin/activate"
python -m pip install --upgrade pip
python -m pip install west

echo "[3/6] Initialize west workspace"
if [[ ! -d "$WS/.west" ]]; then
  west init -m https://github.com/zephyrproject-rtos/zephyr --mr "$ZEPHYR_TAG" "$WS"
fi

cd "$WS"

echo "[4/6] Fetch modules"
west update

echo "[5/6] Export Zephyr CMake package"
west zephyr-export

echo "[6/6] Install Zephyr Python requirements"
python -m pip install -r zephyr/scripts/requirements.txt

cat <<EOF

Bootstrap complete.

Next:
  source "$WS/.venv/bin/activate"
  export ZEPHYR_WS="$WS"
  export BOSSAC="\$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac"
  ./erts/example/mini_beam_esp32/zephyr_app/reflash_nano33_sense.sh --monitor

EOF
