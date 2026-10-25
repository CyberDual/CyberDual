#!/bin/sh
# Build the in-tree expat library with 16-bit XML_Char (XML_UNICODE) and
# AddressSanitizer, compile pocs/residual_be.c against it, and run it.
# Exit status 0 means the PoC fired (AddressSanitizer reported a
# heap-buffer-overflow); 1 means it did not.
set -u
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=$(mktemp -d /tmp/expat-w16-asan.XXXXXX)

cmake "$ROOT/expat" -B "$BUILD" \
  -DEXPAT_CHAR_TYPE=ushort \
  -DEXPAT_BUILD_TESTS=OFF \
  -DEXPAT_BUILD_EXAMPLES=OFF \
  -DEXPAT_BUILD_TOOLS=OFF \
  -DEXPAT_BUILD_FUZZERS=OFF \
  -DEXPAT_SHARED_LIBS=OFF \
  -DCMAKE_C_FLAGS="-g -O1 -fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_BUILD_TYPE= > /dev/null || exit 1
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)" > /dev/null || exit 1

cc -g -O1 -fsanitize=address -fno-omit-frame-pointer \
  -I"$ROOT/expat/lib" "$ROOT/pocs/residual_be.c" "$BUILD"/libexpatw.a \
  -o "$BUILD/residual_be" || exit 1

# The AddressSanitizer runtime in this environment is flaky at process
# start-up (occasionally dies before main); retry those empty crashes and
# only accept a definitive outcome.
i=0
while [ $i -lt 40 ]; do
  i=$((i + 1))
  timeout 60 "$BUILD/residual_be" > "$BUILD/out.log" 2>&1
  if grep -q "heap-buffer-overflow" "$BUILD/out.log"; then
    cat "$BUILD/out.log"
    exit 0
  fi
  if grep -q "all ok" "$BUILD/out.log"; then
    echo "PoC did not fire (parsed cleanly)"
    exit 1
  fi
done
echo "could not get a definitive run"
exit 1
