#!/usr/bin/env bash
set -euo pipefail

LOG_FILE="${1:-}"
CSV_OUT=""
JSON_OUT=""
SCENARIO="default"
shift || true

while [[ $# -gt 0 ]]; do
  case "$1" in
    --csv)
      CSV_OUT="${2:?missing value for --csv}"
      shift 2
      ;;
    --scenario)
      SCENARIO="${2:?missing value for --scenario}"
      shift 2
      ;;
    --json)
      JSON_OUT="${2:?missing value for --json}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$LOG_FILE" ]]; then
  echo "usage: $0 <log-file> [--csv <out.csv>] [--json <out.json>] [--scenario <name>]" >&2
  exit 2
fi

if [[ ! -f "$LOG_FILE" ]]; then
  echo "error: log file not found: $LOG_FILE" >&2
  exit 2
fi

TMP_CLEAN="$(mktemp)"
TMP_PERIODS="$(mktemp)"
trap 'rm -f "$TMP_CLEAN" "$TMP_PERIODS"' EXIT

# Strip ANSI terminal color sequences from captured serial logs.
sed -r 's/\x1B\[[0-9;]*[A-Za-z]//g' "$LOG_FILE" > "$TMP_CLEAN"

declare -A METRICS=()

record_metric() {
  local key="$1"
  local value="$2"
  METRICS["$key"]="$value"
}

AWK_OUT="$(awk -v periods_file="$TMP_PERIODS" '
  /event sensor_id=/ {
    sid=""; ts=""; st=""; inj="";
    for (i = 1; i <= NF; i++) {
      if ($i ~ /^sensor_id=/) { split($i, a, "="); sid = a[2] + 0; }
      if ($i ~ /^ts=/)        { split($i, a, "="); ts = a[2] + 0; }
      if ($i ~ /^status=/)    { split($i, a, "="); st = a[2] + 0; }
      if ($i ~ /^inj=/)       { split($i, a, "="); inj = a[2] + 0; }
    }
    if (sid == "" || ts == "" || st == "") {
      next;
    }
    total_events++;
    sensor_count[sid]++;
    status_count[st]++;
    if (inj == 1) {
      inj_count++;
    }
    if (ts_min == 0 || ts < ts_min) {
      ts_min = ts;
    }
    if (ts > ts_max) {
      ts_max = ts;
    }
    if (last_ts[sid] > 0) {
      period = (ts - last_ts[sid]);
      sensor_period_sum[sid] += period;
      sensor_period_n[sid]++;
      print sid, period >> periods_file;
    }
    last_ts[sid] = ts;
  }
  END {
    print "events_total=" (total_events + 0);
    print "ts_min_ms=" (ts_min + 0);
    print "ts_max_ms=" (ts_max + 0);
    if (ts_max > ts_min) {
      duration = ts_max - ts_min;
      print "duration_ms=" duration;
      printf("event_rate_hz=%.3f\n", (total_events * 1000.0) / duration);
    } else {
      print "duration_ms=0";
      print "event_rate_hz=0";
    }
    print "inj_events=" (inj_count + 0);
    print "status_counts:";
    for (st in status_count) {
      print "  status_" st "=" status_count[st];
    }
    print "sensor_counts:";
    for (sid in sensor_count) {
      print "  sensor_" sid "_events=" sensor_count[sid];
    }
    print "sensor_period_ms_avg:";
    for (sid in sensor_period_n) {
      if (sensor_period_n[sid] > 0) {
        printf("  sensor_%s_avg_ms=%.2f\n", sid, sensor_period_sum[sid] / sensor_period_n[sid]);
      }
    }
  }' "$TMP_CLEAN")"

echo "$AWK_OUT"
while IFS= read -r line; do
  case "$line" in
    events_total=*|ts_min_ms=*|ts_max_ms=*|duration_ms=*|event_rate_hz=*|inj_events=*)
      key="${line%%=*}"
      val="${line#*=}"
      record_metric "$key" "$val"
      ;;
    "  status_"*|"  sensor_"*_events=*|"  sensor_"*_avg_ms=*)
      trimmed="${line#"  "}"
      key="${trimmed%%=*}"
      val="${trimmed#*=}"
      record_metric "$key" "$val"
      ;;
  esac
done <<< "$AWK_OUT"

echo "sensor_period_ms_pct:"
while IFS= read -r sid; do
  mapfile -t periods < <(awk -v s="$sid" '$1 == s { print $2 }' "$TMP_PERIODS" | sort -n)
  n="${#periods[@]}"
  if (( n == 0 )); then
    continue
  fi
  p50i=$(( (50 * n + 99) / 100 ))
  p95i=$(( (95 * n + 99) / 100 ))
  p99i=$(( (99 * n + 99) / 100 ))
  (( p50i < 1 )) && p50i=1
  (( p95i < 1 )) && p95i=1
  (( p99i < 1 )) && p99i=1
  (( p50i > n )) && p50i=$n
  (( p95i > n )) && p95i=$n
  (( p99i > n )) && p99i=$n
  p50="${periods[$((p50i - 1))]}"
  p95="${periods[$((p95i - 1))]}"
  p99="${periods[$((p99i - 1))]}"
  echo "  sensor_${sid}_p50_ms=${p50} sensor_${sid}_p95_ms=${p95} sensor_${sid}_p99_ms=${p99}"
  record_metric "sensor_${sid}_p50_ms" "$p50"
  record_metric "sensor_${sid}_p95_ms" "$p95"
  record_metric "sensor_${sid}_p99_ms" "$p99"
done < <(awk '{print $1}' "$TMP_PERIODS" | sort -u)

first_mb="$(rg 'mb_stats attempted=' "$TMP_CLEAN" | head -n 1 || true)"
last_mb="$(rg 'mb_stats attempted=' "$TMP_CLEAN" | tail -n 1 || true)"
if [[ -n "$first_mb" ]]; then
  echo "mb_stats_first: $first_mb"
fi
if [[ -n "$last_mb" ]]; then
  echo "mb_stats_last: $last_mb"
fi

extract_mb_num() {
  local line="$1"
  local key="$2"
  sed -nE "s/.*${key}=([0-9]+).*/\\1/p" <<<"$line"
}

if [[ -n "$last_mb" ]]; then
  attempted="$(extract_mb_num "$last_mb" "attempted")"
  pushed="$(extract_mb_num "$last_mb" "pushed")"
  dropped_full="$(extract_mb_num "$last_mb" "dropped_full")"
  processed="$(extract_mb_num "$last_mb" "processed")"
  if [[ -n "$attempted" && "$attempted" -gt 0 ]]; then
    util_pct="$(awk -v p="$processed" -v a="$attempted" 'BEGIN { printf "%.2f", (p*100.0)/a }')"
    drop_pct="$(awk -v d="$dropped_full" -v a="$attempted" 'BEGIN { printf "%.2f", (d*100.0)/a }')"
    echo "mb_processed_over_attempted_pct=$util_pct"
    echo "mb_drop_over_attempted_pct=$drop_pct"
    record_metric "mb_processed_over_attempted_pct" "$util_pct"
    record_metric "mb_drop_over_attempted_pct" "$drop_pct"
  fi
fi

if [[ -n "$first_mb" && -n "$last_mb" ]]; then
  first_attempted="$(extract_mb_num "$first_mb" "attempted")"
  first_processed="$(extract_mb_num "$first_mb" "processed")"
  last_attempted="$(extract_mb_num "$last_mb" "attempted")"
  last_processed="$(extract_mb_num "$last_mb" "processed")"
  if [[ -n "$first_attempted" && -n "$last_attempted" && "$last_attempted" -gt "$first_attempted" ]]; then
    delta_a=$((last_attempted - first_attempted))
    delta_p=$((last_processed - first_processed))
    echo "mb_delta_attempted=$delta_a"
    echo "mb_delta_processed=$delta_p"
    record_metric "mb_delta_attempted" "$delta_a"
    record_metric "mb_delta_processed" "$delta_p"
  fi
fi

if [[ -n "$CSV_OUT" ]]; then
  {
    echo "scenario,metric,value"
    for key in "${!METRICS[@]}"; do
      echo "${SCENARIO},${key},${METRICS[$key]}"
    done | sort
  } > "$CSV_OUT"
  echo "csv_written=$CSV_OUT"
fi

if [[ -n "$JSON_OUT" ]]; then
  {
    echo "{"
    printf '  "scenario": "%s",\n' "$SCENARIO"
    echo '  "metrics": {'
    mapfile -t sorted_keys < <(printf '%s\n' "${!METRICS[@]}" | sort)
    for idx in "${!sorted_keys[@]}"; do
      key="${sorted_keys[$idx]}"
      val="${METRICS[$key]}"
      comma=","
      if (( idx == ${#sorted_keys[@]} - 1 )); then
        comma=""
      fi
      printf '    "%s": "%s"%s\n' "$key" "$val" "$comma"
    done
    echo "  }"
    echo "}"
  } > "$JSON_OUT"
  echo "json_written=$JSON_OUT"
fi
