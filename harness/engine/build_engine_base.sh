#!/usr/bin/env bash
# build_engine_base.sh: build the V8 / SpiderMonkey base images from source.
#
# Produces cyberdual-v8-base (V8 at the last commit before 2026-09-24) and cyberdual-sm-base (SpiderMonkey at the last
# commit before 2026-09-17), each with the debug + ASan shell prebuilt so attacked and reviewed trees rebuild
# incrementally. HEAVY: each is a full engine build (hours, tens of GB). `./cyberdual engine images --pull` downloads
# the published images instead.
#
# USAGE:  NINJA_JOBS=8 bash harness/engine/build_engine_base.sh [v8|sm|both]
# ENV:    V8_BASE_DATE=2026-09-24  SM_BASE_DATE=2026-09-17  NINJA_JOBS=$(nproc)  MIN_FREE_GB=120
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WHICH="${1:-both}"
V8_BASE_DATE="${V8_BASE_DATE:-2026-09-24}"; SM_BASE_DATE="${SM_BASE_DATE:-2026-09-17}"
NINJA_JOBS="${NINJA_JOBS:-$(nproc)}"; MIN_FREE_GB="${MIN_FREE_GB:-120}"

root="$(docker info --format '{{.DockerRootDir}}' 2>/dev/null || echo /)"
free_gb=$(df -BG --output=avail "$root" 2>/dev/null | tail -1 | tr -dc '0-9')
echo ">> $root free: ${free_gb:-?}G (need >= ${MIN_FREE_GB}G for an engine build)"
if [ "${free_gb:-0}" -lt "$MIN_FREE_GB" ]; then
  echo "!! not enough disk for an engine build; free space or set MIN_FREE_GB if you know the build fits" >&2
  exit 2
fi

build_v8 () {
  echo "== building cyberdual-v8-base (base < $V8_BASE_DATE, jobs=$NINJA_JOBS) =="
  docker build -t cyberdual-v8-base \
    --build-arg BASE_DATE="$V8_BASE_DATE" --build-arg NINJA_JOBS="$NINJA_JOBS" \
    -f "$HERE/Dockerfile.v8" "$HERE"
}
build_sm () {
  echo "== building cyberdual-sm-base (base < $SM_BASE_DATE) =="
  docker build -t cyberdual-sm-base --build-arg BASE_DATE="$SM_BASE_DATE" -f "$HERE/Dockerfile.sm" "$HERE"
}

rc=0
case "$WHICH" in
  v8) build_v8 || rc=1;;
  sm) build_sm || rc=1;;
  both) build_v8 || rc=1; build_sm || rc=1;;
  *) echo "usage: build_engine_base.sh [v8|sm|both]" >&2; exit 2;;
esac
docker images --format '{{.Repository}}:{{.Tag}} {{.Size}}' | grep -E '^cyberdual-(v8|sm)-base' || true
exit "$rc"
