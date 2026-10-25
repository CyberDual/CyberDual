#!/usr/bin/env bash
# Review an already validated engine cell after an earlier reviewer-stage failure.
set -euo pipefail
cell_dir="${1:?pass an engine cell directory}"
cd "$(dirname "$0")/../.."
cell_dir="$(realpath "$cell_dir")"
ENGINE_NAME="$(basename "$cell_dir" | sed -n 's/^engine_\(sm\|v8\)_.*/\1/p')"
[ -n "$ENGINE_NAME" ] || { echo "not an engine cell: $cell_dir" >&2; exit 2; }
python3 - "$cell_dir/grade.json" <<'PY'
import json,sys
assert json.load(open(sys.argv[1])).get('verdict') == 'VALID_INJECTION', 'cell is not a valid injection'
PY
SHIPPED_IMG="$(cat "$cell_dir/shipped_image.txt")"
# a shipped image removed from Docker is loaded back from the engine image store; a missing one is infrastructure
python3 harness/engine/engine_common.py ensure-image "$SHIPPED_IMG" \
  || { echo "INFRA: shipped image $SHIPPED_IMG is neither in Docker nor in the image store" >&2; exit 3; }

SOURCE_ONLY=1
ENGINE="$ENGINE_NAME"
ATT_MODEL="${ATT_MODEL:-gpt-6-sol}"   # only names the attacker; a replay makes no attacker call
REVIEW=1
source harness/engine/run_engine_window.sh
unset SOURCE_ONLY
engine_cfg "$ENGINE_NAME"
SINCE="${SINCE:-$DEF_SINCE}"
WTAG="${SINCE//-/}_${UNTIL//-/}"
[ "$ENGINE_NAME" != sm ] || WTAG="${WTAG}_jssrc"
TASKID="window_${WTAG}"
CLEAN_IMG="revbench-engine-maint:${ENGINE_NAME}_${WTAG}"
python3 harness/engine/engine_common.py ensure-image "$CLEAN_IMG" \
  || { echo "INFRA: substrate $CLEAN_IMG is neither in Docker nor in the image store" >&2; exit 3; }
verify_window "$CLEAN_IMG" || { echo "REFUSED: $CLEAN_IMG does not match the frozen window" >&2; exit 3; }
k="$(basename "$cell_dir" | sed -n 's/.*_k\([0-9][0-9]*\)_.*/\1/p')"
[ -n "$k" ] || exit 2
run_review "$k" "$cell_dir" "$SHIPPED_IMG"
