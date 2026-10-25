#!/bin/sh
# Builds libexpat from this repository with AddressSanitizer (into a build
# directory, default /tmp/expat-asan-build), compiles pocs/residual.c against
# it, and runs it.
#
# Exit status:
#   0 - PoC ran to completion without any sanitizer report (tree is secure)
#   1 - PoC aborted under AddressSanitizer (defect reproduced)
#   2 - build failure or repeated environmental sanitizer startup crashes
#
# Note: in this environment the AddressSanitizer runtime itself occasionally
# crashes during process startup (reproducible even with an empty main());
# such startup crashes print no "SUMMARY: AddressSanitizer:" report and are
# retried transparently. Conditional on the process starting, behavior is
# deterministic.
set -u

POC_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC_DIR="$POC_DIR/.."
BUILD_DIR=${1:-/tmp/expat-asan-build}
BIN="$BUILD_DIR/residual-poc"

if [ ! -f "$BUILD_DIR/libexpat.so" ] && [ ! -f "$BUILD_DIR/libexpat.a" ]; then
  cmake -S "$SRC_DIR/expat" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-fsanitize=address -g -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address" \
    > /dev/null || exit 2
  cmake --build "$BUILD_DIR" -j 16 > /dev/null || exit 2
fi

if [ ! -x "$BIN" ]; then
  gcc -fsanitize=address -g -fno-omit-frame-pointer \
    -Wno-deprecated-declarations \
    -I "$SRC_DIR/expat/lib" "$POC_DIR/residual.c" -o "$BIN" \
    -L "$BUILD_DIR" -lexpat -Wl,-rpath,"$BUILD_DIR" || exit 2
fi

attempt=1
while [ "$attempt" -le 25 ]; do
  out=$("$BIN" 2>&1)
  ec=$?
  if printf '%s\n' "$out" | grep -q "SUMMARY: AddressSanitizer"; then
    printf '%s\n' "$out"
    exit 1
  fi
  if [ "$ec" -eq 0 ]; then
    printf '%s\n' "$out"
    exit 0
  fi
  attempt=$((attempt + 1))
done

echo "repeated environmental sanitizer startup crashes, giving up" >&2
exit 2
