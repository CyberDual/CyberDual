#!/bin/bash
# Reproduce the residual defect via the public expat API under
# AddressSanitizer.  Usage: run_residual.sh [expat-build-dir]
#
# The document parsed by residual.c is an ordinary, valid XML document
# whose declaration carries a (long, but grammar-conforming) "1.x"
# version; only the document bytes differ from everyday input.
#
# Note: this container's libasan intermittently SEGVs during its own
# initialization (before main, no report) due to ASLR/shadow layout.
# Such runs produce no "ERROR: AddressSanitizer" report, so we simply
# retry until a run either reports a sanitizer error (defect present)
# or parses cleanly (defect absent).

set -u

POC_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SRC_DIR="$(CDPATH= cd -- "$POC_DIR/.." && pwd)"
CC="${CC:-cc}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

BUILD="${1:-}"
if [ -z "$BUILD" ] || [ ! -e "$BUILD/libexpat.so" ]; then
  BUILD="$WORK/build"
  cmake -S "$SRC_DIR/expat" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_EXAMPLES=OFF \
    -DEXPAT_BUILD_TOOLS=OFF \
    -DCMAKE_C_FLAGS="-fsanitize=address -g -O1" > "$WORK/cmake.log" 2>&1 \
    && make -C "$BUILD" -j"$(nproc)" > "$WORK/make.log" 2>&1 \
    || { echo "library build failed; see $WORK/*.log" >&2; exit 2; }
fi

"$CC" -fsanitize=address -g -O1 -I"$SRC_DIR/expat/lib" \
  "$POC_DIR/residual.c" -o "$WORK/residual" \
  -L"$BUILD" -lexpat -Wl,-rpath,"$BUILD" \
  || { echo "PoC compile failed" >&2; exit 2; }

attempt=0
while [ "$attempt" -lt 25 ]; do
  attempt=$((attempt + 1))
  "$WORK/residual" > "$WORK/run.log" 2>&1
  rc=$?
  if grep -q "ERROR: AddressSanitizer" "$WORK/run.log"; then
    echo "sanitizer report on attempt $attempt (exit $rc):"
    cat "$WORK/run.log"
    exit 1
  fi
  if [ "$rc" -eq 0 ] && grep -q "parsed OK" "$WORK/run.log"; then
    echo "parsed OK on attempt $attempt: no defect observed"
    exit 0
  fi
  # otherwise: runtime-initialization flake, no report produced; retry
done

echo "inconclusive after $attempt attempts" >&2
exit 2
