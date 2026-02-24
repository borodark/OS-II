#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
THRESHOLDS_FILE="${THRESHOLDS_FILE:-$REPO_ROOT/system/config/perf_gate_rc.env}"
NOMINAL_CSV="${1:-$REPO_ROOT/system/doc/M5_BASELINE_NOMINAL_SOAK.csv}"
RECOVERY_CSV="${2:-$REPO_ROOT/system/doc/M5_BASELINE_RECOVERY.csv}"

if [[ ! -f "$THRESHOLDS_FILE" ]]; then
  echo "error: thresholds file not found: $THRESHOLDS_FILE" >&2
  exit 2
fi

# shellcheck disable=SC1090
source "$THRESHOLDS_FILE"

"$SCRIPT_DIR/check_perf_regression.sh" "$NOMINAL_CSV" \
  --scenario "$RC_NOMINAL_SCENARIO" \
  --min-event-rate-hz "$RC_NOMINAL_MIN_EVENT_RATE_HZ" \
  --max-drop-pct "$RC_NOMINAL_MAX_DROP_PCT" \
  --min-processed-pct "$RC_NOMINAL_MIN_PROCESSED_PCT" \
  --max-sensor-p99-ms "$RC_NOMINAL_MAX_SENSOR_P99_MS"

"$SCRIPT_DIR/check_perf_regression.sh" "$RECOVERY_CSV" \
  --scenario "$RC_RECOVERY_SCENARIO" \
  --min-event-rate-hz "$RC_RECOVERY_MIN_EVENT_RATE_HZ" \
  --max-drop-pct "$RC_RECOVERY_MAX_DROP_PCT" \
  --min-processed-pct "$RC_RECOVERY_MIN_PROCESSED_PCT" \
  --max-sensor-p99-ms "$RC_RECOVERY_MAX_SENSOR_P99_MS"

echo "PASS: RC perf gate profile satisfied"
