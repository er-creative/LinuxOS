#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="${1:-$(pwd)}"
ROOT="$(cd "$ROOT" && pwd -P)"
cd "$ROOT"

for dir in output/phase319_3a output/phase319_3b; do
  [[ -d "$dir" ]] || { echo "ERROR: Missing $dir" >&2; exit 1; }
done
for f in \
 output/phase319_3a/session_integrity.csv \
 output/phase319_3a/missing_candles.csv \
 output/phase319_3a/file_issues.csv \
 output/phase319_3a/timeframe_comparisons.csv \
 output/phase319_3b/daily_eligibility.csv \
 output/phase319_3b/decision_eligibility.csv \
 output/phase319_3b/universe_readiness.csv; do
  [[ -s "$f" ]] || { echo "ERROR: Missing/empty $f" >&2; exit 1; }
done

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
DEST="output/validation_baselines/phase319_${STAMP}"
mkdir -p "$DEST/reports/phase319_3a" "$DEST/reports/phase319_3b" "$DEST/source_snapshot" "$DEST/logs"
cp -p output/phase319_3a/*.csv "$DEST/reports/phase319_3a/"
cp -p output/phase319_3b/*.csv "$DEST/reports/phase319_3b/"

SOURCES=(
 tools/cete_historical_gap_classifier.cpp
 tools/cete_replay_readiness.cpp
 include/cete/replay/ReplayEligibilityGate.hpp
 src/replay/ReplayEligibilityGate.cpp
 tests/replay/test_replay_eligibility_gate.cpp
)
for f in "${SOURCES[@]}"; do
  if [[ -f "$f" ]]; then
    mkdir -p "$DEST/source_snapshot/$(dirname "$f")"
    cp -p "$f" "$DEST/source_snapshot/$f"
  else
    printf 'Not present: %s\n' "$f" >> "$DEST/logs/missing_optional_sources.txt"
  fi
done

{
  printf 'UTC snapshot: %s\n' "$STAMP"
  printf 'Project root: %s\n' "$ROOT"
  printf 'Git commit: '
  git rev-parse HEAD 2>/dev/null || printf 'Not available\n'
  printf 'Git status (short):\n'
  git status --short 2>/dev/null || true
  printf '\nCompiler:\n'
  g++ --version | head -n 1 || true
  printf '\nValidation reports:\n'
  find "$DEST/reports" -type f -name '*.csv' -printf '%P (%s bytes)\n' | sort
} > "$DEST/logs/manifest.txt"

if [[ -x build/test_replay_eligibility_gate ]]; then
  set +e
  ./build/test_replay_eligibility_gate output/phase319_3b > "$DEST/logs/phase319_3c_test.txt" 2>&1
  TEST_EXIT=$?
  set -e
  printf '%s\n' "$TEST_EXIT" > "$DEST/logs/phase319_3c_exit_code.txt"
  if (( TEST_EXIT != 0 )); then
    echo "WARNING: Phase 3.19.3C test exited $TEST_EXIT; snapshot retained." >&2
  fi
else
  printf 'NOT_RUN: build/test_replay_eligibility_gate missing or not executable\n' > "$DEST/logs/phase319_3c_test.txt"
fi

(
  cd "$DEST"
  find reports source_snapshot logs -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 -r sha256sum > SHA256SUMS
)

printf '\nBaseline saved: %s\n' "$DEST"
printf 'Files copied: '
find "$DEST/reports" -type f -name '*.csv' | wc -l
printf 'Integrity check: '
(cd "$DEST" && sha256sum -c SHA256SUMS >/dev/null && echo PASS)
printf '\nNOTE: This snapshot records the existing conservative timing behavior; it does not certify hybrid eligibility.\n'
