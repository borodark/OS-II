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

TMP_CLEAN="$(mktemp)"
trap 'rm -f "$TMP_CLEAN"' EXIT

# Strip ANSI terminal color sequences from captured serial logs.
sed -r 's/\x1B\[[0-9;]*[A-Za-z]//g' "$LOG_FILE" > "$TMP_CLEAN"

awk '
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
      sensor_period_sum[sid] += (ts - last_ts[sid]);
      sensor_period_n[sid]++;
    }
    last_ts[sid] = ts;
  }
  END {
    print "events_total=" total_events;
    print "ts_min_ms=" ts_min;
    print "ts_max_ms=" ts_max;
    if (ts_max > ts_min) {
      duration = ts_max - ts_min;
      print "duration_ms=" duration;
      printf("event_rate_hz=%.3f\n", (total_events * 1000.0) / duration);
    } else {
      print "duration_ms=0";
      print "event_rate_hz=0";
    }
    print "inj_events=" inj_count;
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
  }
' "$TMP_CLEAN"

last_mb="$(rg 'mb_stats attempted=' "$TMP_CLEAN" | tail -n 1 || true)"
if [[ -n "$last_mb" ]]; then
  echo "mb_stats_last: $last_mb"
fi
