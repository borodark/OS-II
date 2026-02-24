#!/usr/bin/env bash
set -euo pipefail

LOG_FILE="${1:-}"
if [[ -z "$LOG_FILE" ]]; then
  echo "usage: $0 <log-file>" >&2
  exit 2
fi

if [[ ! -f "$LOG_FILE" ]]; then
  echo "error: log file not found: $LOG_FILE" >&2
  exit 2
fi

count_or_zero() {
  local pattern="$1"
  rg -c -- "$pattern" "$LOG_FILE" 2>/dev/null || echo 0
}

boot_count="$(count_or_zero '\*\*\* Booting Zephyr OS')"
status4_count="$(count_or_zero 'status=4')"
status5_count="$(count_or_zero 'status=5')"
wdt_hold_count="$(count_or_zero 'withholding task_wdt feed')"
boot_counter_count="$(count_or_zero 'boot counter=')"
mapfile -t boot_counters < <(rg -o --no-line-number 'boot counter=[0-9]+' "$LOG_FILE" | sed -E 's/.*=([0-9]+)/\1/' || true)
boot_counter_first="${boot_counters[0]:-}"
boot_counter_second="${boot_counters[1]:-}"

echo "log: $LOG_FILE"
echo "boot banners: $boot_count"
echo "status=4 lines: $status4_count"
echo "status=5 lines: $status5_count"
echo "wdt hold lines: $wdt_hold_count"
echo "boot counter lines: $boot_counter_count"
if [[ -n "$boot_counter_first" ]]; then
  echo "boot counter first: $boot_counter_first"
fi
if [[ -n "$boot_counter_second" ]]; then
  echo "boot counter second: $boot_counter_second"
fi

if (( status4_count < 1 )); then
  echo "FAIL: missing retry evidence (status=4)" >&2
  exit 1
fi
if (( status5_count < 1 )); then
  echo "FAIL: missing degraded evidence (status=5)" >&2
  exit 1
fi
if (( wdt_hold_count < 1 )); then
  echo "FAIL: missing watchdog hold evidence" >&2
  exit 1
fi
if (( boot_count < 2 )); then
  echo "FAIL: expected reboot (at least two boot banners)" >&2
  exit 1
fi
if (( boot_counter_count < 2 )); then
  echo "FAIL: expected boot counter to be logged across reboot" >&2
  exit 1
fi
if [[ -z "$boot_counter_first" || -z "$boot_counter_second" ]]; then
  echo "FAIL: unable to parse first two boot counter values" >&2
  exit 1
fi
if (( boot_counter_second <= boot_counter_first )); then
  echo "FAIL: boot counter did not increase across reboot (${boot_counter_first} -> ${boot_counter_second})" >&2
  exit 1
fi

echo "PASS: degraded -> watchdog -> reboot path is present and counter increments."
