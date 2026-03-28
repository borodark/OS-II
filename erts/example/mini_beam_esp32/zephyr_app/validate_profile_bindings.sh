#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
usage: validate_profile_bindings.sh --profile <profile.os2> --log <boot.log>
EOF
}

profile=""
log_file=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      profile="${2:-}"
      shift 2
      ;;
    --log)
      log_file="${2:-}"
      shift 2
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [[ -z "$profile" || -z "$log_file" ]]; then
  usage
  exit 2
fi

if [[ ! -f "$profile" ]]; then
  echo "ERROR: profile not found: $profile" >&2
  exit 2
fi

if [[ ! -f "$log_file" ]]; then
  echo "ERROR: log file not found: $log_file" >&2
  exit 2
fi

caps_line="$(grep -m1 'os2_caps_v1 #{' "$log_file" || true)"
if [[ -z "$caps_line" ]]; then
  echo "ERROR: os2_caps_v1 term not found in log: $log_file" >&2
  exit 1
fi

extract_num() {
  local key="$1"
  sed -n "s/.*${key}=>\\([0-9][0-9]*\\).*/\\1/p" <<<"$caps_line" | head -n1
}

extract_word() {
  local key="$1"
  sed -n "s/.*${key}=>\\([a-zA-Z0-9_]*\\).*/\\1/p" <<<"$caps_line" | head -n1
}

caps_board="$(extract_word board)"
caps_vm="$(extract_word vm)"
caps_event_schema="$(extract_num event_schema)"
caps_mailbox_depth="$(extract_num mailbox_depth)"
caps_i2c="$(extract_num i2c)"
caps_spi="$(extract_num spi)"
caps_pwm="$(extract_num pwm)"
caps_adc_channels="$(sed -n 's/.*adc=>#{channels=>\([0-9][0-9]*\),max_ksps=>[0-9][0-9]*}.*/\1/p' <<<"$caps_line" | head -n1)"
caps_adc_max_ksps="$(sed -n 's/.*adc=>#{channels=>[0-9][0-9]*,max_ksps=>\([0-9][0-9]*\)}.*/\1/p' <<<"$caps_line" | head -n1)"
caps_qdec="$(extract_num qdec)"
caps_i2s="$(extract_num i2s)"
caps_pdm="$(extract_num pdm)"
caps_ble="$(extract_num ble)"
caps_policy="$(extract_word policy)"
caps_power_domains_raw="$(sed -n 's/.*power_domains=>\[\([^]]*\)\].*/\1/p' <<<"$caps_line" | head -n1)"

for field in caps_board caps_vm caps_event_schema caps_mailbox_depth caps_i2c caps_spi caps_pwm caps_adc_channels caps_adc_max_ksps caps_qdec caps_i2s caps_pdm caps_ble caps_policy; do
  if [[ -z "${!field}" ]]; then
    echo "ERROR: failed to parse '$field' from capability line" >&2
    echo "$caps_line" >&2
    exit 1
  fi
done

declare -A cfg
while IFS='=' read -r key value; do
  [[ -z "$key" ]] && continue
  key="${key%%[[:space:]]*}"
  [[ -z "$key" ]] && continue
  [[ "$key" == \#* ]] && continue
  value="${value:-}"
  value="$(sed 's/^[[:space:]]*//; s/[[:space:]]*$//' <<<"$value")"
  cfg["$key"]="$value"
done < "$profile"

need_key() {
  local k="$1"
  if [[ -z "${cfg[$k]:-}" ]]; then
    echo "ERROR: missing required profile key: $k" >&2
    exit 1
  fi
}

need_key profile_v
need_key board
need_key require.vm
need_key require.event_schema
need_key require.policy
need_key require.i2c_min
need_key require.pwm_min
need_key require.mailbox_depth_min
need_key bind.sensor_bus
need_key bind.pwm_channel

if [[ "${cfg[profile_v]}" != "1" ]]; then
  echo "ERROR: unsupported profile_v=${cfg[profile_v]} (expected 1)" >&2
  exit 1
fi

if [[ "${cfg[board]}" != "$caps_board" ]]; then
  echo "ERROR: board mismatch profile=${cfg[board]} caps=$caps_board" >&2
  exit 1
fi

if [[ "${cfg[require.vm]}" != "$caps_vm" ]]; then
  echo "ERROR: vm mismatch profile=${cfg[require.vm]} caps=$caps_vm" >&2
  exit 1
fi

if [[ "${cfg[require.policy]}" != "$caps_policy" ]]; then
  echo "ERROR: policy mismatch profile=${cfg[require.policy]} caps=$caps_policy" >&2
  exit 1
fi

if (( caps_event_schema < cfg[require.event_schema] )); then
  echo "ERROR: event schema too old caps=$caps_event_schema require=${cfg[require.event_schema]}" >&2
  exit 1
fi

if (( caps_i2c < cfg[require.i2c_min] )); then
  echo "ERROR: i2c buses insufficient caps=$caps_i2c require=${cfg[require.i2c_min]}" >&2
  exit 1
fi

if (( caps_pwm < cfg[require.pwm_min] )); then
  echo "ERROR: pwm channels insufficient caps=$caps_pwm require=${cfg[require.pwm_min]}" >&2
  exit 1
fi

if (( caps_mailbox_depth < cfg[require.mailbox_depth_min] )); then
  echo "ERROR: mailbox depth insufficient caps=$caps_mailbox_depth require=${cfg[require.mailbox_depth_min]}" >&2
  exit 1
fi

if [[ -n "${cfg[require.adc_channels_min]:-}" ]] && (( caps_adc_channels < cfg[require.adc_channels_min] )); then
  echo "ERROR: adc channels insufficient caps=$caps_adc_channels require=${cfg[require.adc_channels_min]}" >&2
  exit 1
fi

if [[ -n "${cfg[require.ble]:-}" ]] && (( caps_ble < cfg[require.ble] )); then
  echo "ERROR: BLE support insufficient caps=$caps_ble require=${cfg[require.ble]}" >&2
  exit 1
fi

if [[ -n "${cfg[require.i2s]:-}" ]] && (( caps_i2s < cfg[require.i2s] )); then
  echo "ERROR: I2S support insufficient caps=$caps_i2s require=${cfg[require.i2s]}" >&2
  exit 1
fi

if [[ -n "${cfg[require.pdm]:-}" ]] && (( caps_pdm < cfg[require.pdm] )); then
  echo "ERROR: PDM support insufficient caps=$caps_pdm require=${cfg[require.pdm]}" >&2
  exit 1
fi

if [[ -n "${cfg[require.qdec]:-}" ]] && (( caps_qdec < cfg[require.qdec] )); then
  echo "ERROR: QDEC support insufficient caps=$caps_qdec require=${cfg[require.qdec]}" >&2
  exit 1
fi

bind_sensor_bus="${cfg[bind.sensor_bus]}"
bind_pwm_channel="${cfg[bind.pwm_channel]}"
if (( bind_sensor_bus >= caps_i2c )); then
  echo "ERROR: bind.sensor_bus=$bind_sensor_bus out of range (i2c buses=$caps_i2c)" >&2
  exit 1
fi

if (( bind_pwm_channel >= caps_pwm )); then
  echo "ERROR: bind.pwm_channel=$bind_pwm_channel out of range (pwm channels=$caps_pwm)" >&2
  exit 1
fi

if [[ -n "${cfg[bind.power_domains]:-}" ]]; then
  IFS=',' read -r -a domains <<<"${cfg[bind.power_domains]}"
  for d in "${domains[@]}"; do
    d="$(sed 's/^[[:space:]]*//; s/[[:space:]]*$//' <<<"$d")"
    [[ -z "$d" ]] && continue
    if ! grep -Eq "(^|,)[[:space:]]*${d}([[:space:]]*,|$)" <<<"$caps_power_domains_raw"; then
      echo "ERROR: required power domain '$d' missing from caps [$caps_power_domains_raw]" >&2
      exit 1
    fi
  done
fi

echo "PASS: profile '$profile' is compatible with caps in '$log_file'"
echo "binding_table: board=$caps_board vm=$caps_vm policy=$caps_policy i2c_bus=$bind_sensor_bus pwm_channel=$bind_pwm_channel sensor_addr=${cfg[bind.sensor_addr]:-n/a} sensor_reg=${cfg[bind.sensor_reg]:-n/a} pwm_freq_hz=${cfg[bind.pwm_freq_hz]:-n/a}"
