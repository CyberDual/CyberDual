#!/usr/bin/env bash
# window_ops.sh: the window steps of run_engine_window.sh, for attacks stored in the engine bank.
#
#   window_ops.sh substrate ENGINE                    the frozen window image (built from the base image when missing,
#                                                     then checked against attack-bank/engines/windows/ENGINE/window.json)
#   window_ops.sh shipped ENGINE BUNDLE TAG LOG       the attacked window: BUNDLE's maint-batch on the window image,
#                                                     rebuilt incrementally and committed as TAG
#   window_ops.sh grade ENGINE IMAGE PROBE OUTDIR     the probe's verdict line on IMAGE (engine_common.grade_probe)
#   window_ops.sh review ENGINE OUTDIR SHIPPED REV    scrubbed handoff of SHIPPED, reviewer REV, rebuild, re-grade
#                                                     (writes OUTDIR/review_grade_<REV>_shell.json)
# Every step is the function of run_engine_window.sh that produced the paper's engine results.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cmd="${1:?usage: window_ops.sh substrate|shipped|grade|review ENGINE ...}"; shift
ENGINE_NAME="${1:?engine (v8|sm)}"; shift

SOURCE_ONLY=1 ENGINE="$ENGINE_NAME" REVIEW=0 ATT_MODEL="${ATT_MODEL:-gpt-6-sol}"   # names the attacker only; no attacker call
# shellcheck source=run_engine_window.sh
source "$HERE/run_engine_window.sh" >&2   # its banner goes to stderr; stdout carries results
unset SOURCE_ONLY
engine_cfg "$ENGINE_NAME" || exit 2
SINCE="$DEF_SINCE"
WTAG="${SINCE//-/}_${UNTIL//-/}"
[ "$ENGINE_NAME" != sm ] || WTAG="${WTAG}_jssrc"
TASKID="window_${WTAG}"
CLEAN_IMG="revbench-engine-maint:${ENGINE_NAME}_${WTAG}"

case "$cmd" in
  substrate)
    if ! docker image inspect "$CLEAN_IMG" >/dev/null 2>&1; then FORCE_SUBSTRATE=1; fi
    prepare_substrate >&2 || exit 3
    echo "$CLEAN_IMG"
    ;;
  shipped)
    BUNDLE="$(realpath "${1:?bundle}")"; TAG="${2:?image tag}"; LOG="${3:?build log}"
    docker image inspect "$CLEAN_IMG" >/dev/null 2>&1 || { echo "missing $CLEAN_IMG (run: window_ops.sh substrate $ENGINE_NAME)" >&2; exit 3; }
    CID="cyberdual_engine_ship_${ENGINE_NAME}_$$_${RANDOM}"
    docker rm -f "$CID" >/dev/null 2>&1 || true
    docker run --name "$CID" -d --network none "${BUILD_ENV_OPTS[@]}" -w "$WORKDIR" "$CLEAN_IMG" sleep infinity >/dev/null || exit 3
    trap 'docker rm -f "$CID" >/dev/null 2>&1 || true' EXIT
    docker cp "$BUNDLE" "$CID:/tmp/window.bundle" >/dev/null || exit 3
    # the bundle holds main..maint-batch; its prerequisite is the window base (main), which the window image carries
    docker exec -e REBUILD="$REBUILD" "$CID" sh -lc '
      set -e
      git bundle verify -q /tmp/window.bundle
      git fetch -q /tmp/window.bundle "+refs/heads/maint-batch:refs/heads/attacked"
      test "$(git rev-list --count main..attacked)" = "$(git rev-list --count main..orig_month)"
      git checkout -q -B maint-batch attacked
      git branch -D attacked >/dev/null
      rm -f /tmp/window.bundle
      echo "attacked tip $(git rev-parse HEAD), $(git diff --shortstat orig_month)"
      eval "$REBUILD"
    ' > "$LOG" 2>&1 || { echo "attacked window did not apply or build (see $LOG)" >&2; exit 4; }
    docker commit "$CID" "$TAG" >/dev/null || exit 3
    echo "$TAG"
    ;;
  grade)
    IMAGE="${1:?image}"; PROBE="${2:?probe}"; OUTDIR="${3:?outdir}"
    mkdir -p "$OUTDIR"
    grade_image "$IMAGE" "$PROBE" "$OUTDIR/sbp_${4:-grade}"
    ;;
  review)
    OUTDIR="$(realpath "${1:?outdir}")"; SHIPPED_IMG="${2:?shipped image}"; REV="${3:?reviewer model}"; ARMS=shell
    docker image inspect "$CLEAN_IMG" >/dev/null 2>&1 || { echo "missing $CLEAN_IMG" >&2; exit 3; }
    run_review 1 "$OUTDIR" "$SHIPPED_IMG"
    ;;
  *) echo "unknown command: $cmd" >&2; exit 2 ;;
esac
