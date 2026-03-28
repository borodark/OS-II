#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <log-file>" >&2
  exit 2
fi

log_file="$1"
if [[ ! -f "$log_file" ]]; then
  echo "ERROR: log file not found: $log_file" >&2
  exit 2
fi

line="$(grep -m1 'os2_caps_v1 #{' "$log_file" || true)"
if [[ -z "$line" ]]; then
  echo "ERROR: no os2_caps_v1 term found in $log_file" >&2
  exit 1
fi

required_tokens=(
  "caps_v=>1"
  "board=>"
  "vm=>mini_beam"
  "event_schema=>2"
  "mailbox_depth=>"
  "i2c=>"
  "spi=>"
  "pwm=>"
  "adc=>#{channels=>"
  "max_ksps=>"
  "rtc=>"
  "timers32=>"
  "qdec=>"
  "i2s=>"
  "pdm=>"
  "ble=>"
  "easydma=>"
  "policy=>reject_new"
  "power_domains=>"
  "wdt_ms=>"
)

for token in "${required_tokens[@]}"; do
  if ! grep -Fq "$token" <<<"$line"; then
    echo "ERROR: missing token '$token' in capability term" >&2
    echo "$line" >&2
    exit 1
  fi
done

i2c="$(sed -n 's/.*i2c=>\([0-9][0-9]*\).*/\1/p' <<<"$line" | head -n1)"
pwm="$(sed -n 's/.*pwm=>\([0-9][0-9]*\).*/\1/p' <<<"$line" | head -n1)"
mb_depth="$(sed -n 's/.*mailbox_depth=>\([0-9][0-9]*\).*/\1/p' <<<"$line" | head -n1)"

if [[ -z "$i2c" || -z "$pwm" || -z "$mb_depth" ]]; then
  echo "ERROR: failed to parse numeric fields from capability term" >&2
  echo "$line" >&2
  exit 1
fi

if (( i2c < 1 )); then
  echo "ERROR: invalid i2c bus count: $i2c" >&2
  exit 1
fi

if (( pwm < 1 )); then
  echo "ERROR: invalid pwm channel count: $pwm" >&2
  exit 1
fi

if (( mb_depth < 1 )); then
  echo "ERROR: invalid mailbox depth: $mb_depth" >&2
  exit 1
fi

echo "PASS: os2_caps_v1 term found and validated"
echo "$line"
