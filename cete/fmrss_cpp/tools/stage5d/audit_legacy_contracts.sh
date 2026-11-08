#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/../../.."
echo 'Legacy rank-10/SHORT contracts (inspect before migration):'
grep -RInE 'candidate_rank|rank[[:space:]]*([!=]=)[[:space:]]*10|SHORT|short_slot' src/strategy/dev_ai_strategy include/devai/strategy/dev_ai_strategy | grep -E 'DevAgent|DevManager|DevTopCandidate|DevMultiStockOOSValidator|DevHistoricalTrainingCache|DevSharedModelMultiManagerRuntime|DevRealHistoricalCacheGenerator|DevRealHistoricalTrainingDriver' || true
