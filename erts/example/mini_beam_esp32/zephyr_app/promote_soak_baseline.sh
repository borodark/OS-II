#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DOC_DIR="$REPO_ROOT/system/doc"

PROFILE=""
DATE_TAG="$(date +%F)"
INPUT_CSV=""
INPUT_JSON=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      PROFILE="${2:?missing value for --profile}"
      shift 2
      ;;
    --date)
      DATE_TAG="${2:?missing value for --date}"
      shift 2
      ;;
    --csv)
      INPUT_CSV="${2:?missing value for --csv}"
      shift 2
      ;;
    --json)
      INPUT_JSON="${2:?missing value for --json}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$PROFILE" ]]; then
  echo "usage: $0 --profile <10m|30m|60m> [--date YYYY-MM-DD] [--csv <path>] [--json <path>]" >&2
  exit 2
fi

INPUT_CSV="${INPUT_CSV:-$SCRIPT_DIR/logs/nominal_soak_${PROFILE}.csv}"
INPUT_JSON="${INPUT_JSON:-$SCRIPT_DIR/logs/nominal_soak_${PROFILE}.json}"

if [[ ! -f "$INPUT_CSV" ]]; then
  echo "error: csv not found: $INPUT_CSV" >&2
  exit 2
fi
if [[ ! -f "$INPUT_JSON" ]]; then
  echo "error: json not found: $INPUT_JSON" >&2
  exit 2
fi

mkdir -p "$DOC_DIR"

OUT_CSV="$DOC_DIR/M5_BASELINE_NOMINAL_SOAK_${PROFILE}_${DATE_TAG}.csv"
OUT_JSON="$DOC_DIR/M5_BASELINE_NOMINAL_SOAK_${PROFILE}_${DATE_TAG}.json"
LATEST_CSV="$DOC_DIR/M5_BASELINE_NOMINAL_SOAK_${PROFILE}_LATEST.csv"
LATEST_JSON="$DOC_DIR/M5_BASELINE_NOMINAL_SOAK_${PROFILE}_LATEST.json"

cp "$INPUT_CSV" "$OUT_CSV"
cp "$INPUT_JSON" "$OUT_JSON"
cp "$OUT_CSV" "$LATEST_CSV"
cp "$OUT_JSON" "$LATEST_JSON"

MANIFEST="$DOC_DIR/M5_BASELINE_MANIFEST.csv"
if [[ ! -f "$MANIFEST" ]]; then
  echo "profile,date,csv,json" > "$MANIFEST"
fi

tmp_manifest="$(mktemp)"
trap 'rm -f "$tmp_manifest"' EXIT
awk -F, -v p="$PROFILE" -v d="$DATE_TAG" 'NR==1 || !($1==p && $2==d)' "$MANIFEST" > "$tmp_manifest"
printf "%s,%s,%s,%s\n" "$PROFILE" "$DATE_TAG" "$(basename "$OUT_CSV")" "$(basename "$OUT_JSON")" >> "$tmp_manifest"
mv "$tmp_manifest" "$MANIFEST"

echo "promoted: $OUT_CSV"
echo "promoted: $OUT_JSON"
echo "latest:   $LATEST_CSV"
echo "latest:   $LATEST_JSON"
