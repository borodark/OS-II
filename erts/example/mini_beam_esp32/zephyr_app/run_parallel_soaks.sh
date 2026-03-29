#!/usr/bin/env bash
set -euo pipefail

# Run soak profiles across multiple boards concurrently.
# Example:
#   ./run_parallel_soaks.sh --profile 30m --sudo-chown --ports /dev/ttyACM0,/dev/ttyACM1,/dev/ttyACM2
#
# By default this script skips flashing and assumes firmware is already deployed.
# Use --flash-first to flash each port sequentially before parallel capture.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE="10m"
PORTS_CSV=""
SUDO_CHOWN=0
FLASH_FIRST=0
BAUD="115200"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      PROFILE="${2:?missing value for --profile}"
      shift 2
      ;;
    --ports)
      PORTS_CSV="${2:?missing value for --ports}"
      shift 2
      ;;
    --baud)
      BAUD="${2:?missing value for --baud}"
      shift 2
      ;;
    --sudo-chown)
      SUDO_CHOWN=1
      shift
      ;;
    --flash-first)
      FLASH_FIRST=1
      shift
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$PORTS_CSV" ]]; then
  mapfile -t detected_ports < <(ls /dev/ttyACM* 2>/dev/null | sort || true)
  if (( ${#detected_ports[@]} == 0 )); then
    echo "error: no /dev/ttyACM* ports found" >&2
    exit 2
  fi
  PORTS_CSV="$(IFS=,; echo "${detected_ports[*]}")"
fi

IFS=',' read -r -a ports <<< "$PORTS_CSV"
if (( ${#ports[@]} == 0 )); then
  echo "error: empty port list" >&2
  exit 2
fi

echo "parallel soak plan: profile=$PROFILE ports=${ports[*]} flash_first=$FLASH_FIRST"

if [[ "$FLASH_FIRST" -eq 1 ]]; then
  for p in "${ports[@]}"; do
    echo "[flash] $p"
    flash_args=()
    if [[ "$SUDO_CHOWN" -eq 1 ]]; then
      flash_args+=(--sudo-chown)
    fi
    PORT="$p" "$SCRIPT_DIR/reflash_nano33_sense.sh" "${flash_args[@]}"
  done
fi

pids=()
for p in "${ports[@]}"; do
  label="$(basename "$p")"
  args=(--profile "$PROFILE" --port "$p" --baud "$BAUD" --label "$label" --skip-flash)
  if [[ "$SUDO_CHOWN" -eq 1 ]]; then
    args+=(--sudo-chown)
  fi
  echo "[start] $p -> label=$label"
  "$SCRIPT_DIR/run_soak_profile.sh" "${args[@]}" &
  pids+=("$!")
done

fail=0
for pid in "${pids[@]}"; do
  if ! wait "$pid"; then
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "parallel soak: one or more runs failed" >&2
  exit 1
fi

echo "parallel soak: all runs completed"
