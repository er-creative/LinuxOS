#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
grep -RInE 'candidate_rank.*10|rank.?10|slot_index.*4.*10|short_slot|slot 4.*SHORT' src/strategy/dev_ai_strategy include/devai/strategy/dev_ai_strategy tests/strategy/dev_ai_strategy --include='*.cpp' --include='*.hpp' || true
