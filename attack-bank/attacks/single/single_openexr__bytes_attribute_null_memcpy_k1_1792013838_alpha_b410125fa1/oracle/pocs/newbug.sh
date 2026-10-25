#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD=""
for c in \
    "${OPENEXR_BUILD:-}" \
    /tmp/exr-asan \
    "$ROOT/build" \
    "$ROOT/_build" \
    "$ROOT/_build/debug" \
    "$ROOT/_build/release" \
    "$ROOT/out"
do
    if [ -n "$c" ] && [ -f "$c/src/lib/OpenEXR/libOpenEXR-4_0.a" ]; then
        BUILD="$c"
        break
    fi
done
if [ -z "$BUILD" ]; then
    found="$(find "$ROOT" /tmp -name 'libOpenEXR-4_0.a' 2>/dev/null | head -n 1 || true)"
    if [ -n "$found" ]; then
        BUILD="$(cd "$(dirname "$found")/../../.." && pwd)"
    fi
fi
if [ -z "$BUILD" ]; then
    echo "OpenEXR build not found" >&2
    exit 2
fi
INC=(
    -I"$BUILD/src/lib/OpenEXR"
    -I"$ROOT/src/lib/OpenEXR"
    -I"$ROOT/src/lib/OpenEXRCore"
    -I"$ROOT/src/lib/Iex"
    -I"$ROOT/src/lib/IlmThread"
    -I"$BUILD/cmake"
    -I/usr/include/Imath
)
OUT="${TMPDIR:-/tmp}/newbug_poc"
clang++ -std=c++17 -fsanitize=address -fno-omit-frame-pointer -g -O1 \
    "${INC[@]}" "$ROOT/pocs/newbug.cpp" \
    "$BUILD/src/lib/OpenEXR/libOpenEXR-4_0.a" \
    "$BUILD/src/lib/OpenEXRCore/libOpenEXRCore-4_0.a" \
    "$BUILD/src/lib/IlmThread/libIlmThread-4_0.a" \
    "$BUILD/src/lib/Iex/libIex-4_0.a" \
    -lImath -ldeflate -lzstd -lpthread -lm \
    -o "$OUT"
export ASAN_OPTIONS="abort_on_error=1${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
"$OUT"
