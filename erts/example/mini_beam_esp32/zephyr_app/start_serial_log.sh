#!/usr/bin/env bash
set -euo pipefail

# Reliable serial logger for Nano 33 BLE/Sense.
# - Reconnects after USB reset/re-enumeration
# - Appends logs to file
# - Optional best-effort sudo chown for recreated tty device
#
# Usage:
#   ./start_serial_log.sh
#   ./start_serial_log.sh --port /dev/ttyACM1 --log logs/nano33.log
#   ./start_serial_log.sh --sudo-chown

PORT="/dev/ttyACM0"
BAUD="115200"
LOG_FILE="logs/nano33.log"
SUDO_CHOWN=0
BACKEND="${SERIAL_BACKEND:-auto}"
RECONNECT_SLEEP_SEC="${RECONNECT_SLEEP_SEC:-0.2}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="${2:?missing value for --port}"
      shift 2
      ;;
    --baud)
      BAUD="${2:?missing value for --baud}"
      shift 2
      ;;
    --log)
      LOG_FILE="${2:?missing value for --log}"
      shift 2
      ;;
    --sudo-chown)
      SUDO_CHOWN=1
      shift
      ;;
    --backend)
      BACKEND="${2:?missing value for --backend}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

mkdir -p "$(dirname "$LOG_FILE")"
touch "$LOG_FILE"

running=1
cleanup() { running=0; }
trap cleanup INT TERM

log_line() {
  printf "[%s] %s\n" "$(date +'%F %T')" "$1" | tee -a "$LOG_FILE"
}

ensure_port_access() {
  if [[ ! -e "$PORT" ]]; then
    return 1
  fi
  if [[ -r "$PORT" && -w "$PORT" ]]; then
    return 0
  fi
  if [[ "$SUDO_CHOWN" -eq 1 ]]; then
    sudo chown "$USER:$USER" "$PORT" >/dev/null 2>&1 || true
  fi
  [[ -r "$PORT" && -w "$PORT" ]]
}

log_line "serial logger starting: port=$PORT baud=$BAUD log=$LOG_FILE"
log_line "tip: tail -n +1 -F $LOG_FILE --sleep-interval=0.1"

resolve_backend() {
  case "$BACKEND" in
    auto)
      # picocom is suitable for interactive TTY sessions; in non-interactive
      # soak runs prefer cat to avoid immediate picocom exit loops.
      if command -v picocom >/dev/null 2>&1 && [[ -t 0 && -t 1 ]]; then
        echo "picocom"
      else
        echo "cat"
      fi
      ;;
    picocom|cat)
      echo "$BACKEND"
      ;;
    *)
      echo "error: unsupported backend '$BACKEND' (use auto|picocom|cat)" >&2
      exit 2
      ;;
  esac
}

SER_BACKEND="$(resolve_backend)"
if [[ "$SER_BACKEND" == "picocom" ]]; then
  if [[ "${RECONNECT_SLEEP_SEC}" == "0.2" ]]; then
    RECONNECT_SLEEP_SEC="1.0"
  fi
fi
log_line "backend=$SER_BACKEND"

while [[ "$running" -eq 1 ]]; do
  if [[ ! -e "$PORT" ]]; then
    sleep 0.2
    continue
  fi

  if ! ensure_port_access; then
    log_line "waiting for access to $PORT (try: --sudo-chown)"
    sleep 0.5
    continue
  fi

  # Configure tty. If device vanishes during config, retry loop handles it.
  if ! stty -F "$PORT" "$BAUD" cs8 -cstopb -parenb -icanon min 1 time 0 -echo 2>/dev/null; then
    sleep 0.2
    continue
  fi

  log_line "connected: $PORT"

  # Keep backend one-shot: when device disconnects/reset happens,
  # process exits and outer loop reconnects.
  if [[ "$SER_BACKEND" == "picocom" ]]; then
    # picocom asserts modem control lines more reliably on CDC ACM links.
    # Use noinit/noreset to reduce line toggles on reconnect loops.
    if ! stdbuf -oL -eL picocom -q --noinit --noreset --nolock -b "$BAUD" "$PORT" 2>/dev/null \
      | stdbuf -oL -eL tee -a "$LOG_FILE" >/dev/null; then
      :
    fi
  else
    if ! cat "$PORT" | stdbuf -oL -eL tee -a "$LOG_FILE" >/dev/null; then
      :
    fi
  fi

  log_line "disconnected: $PORT (reconnecting...)"
  sleep "$RECONNECT_SLEEP_SEC"
done

log_line "serial logger stopped"
