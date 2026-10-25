#!/bin/bash
# Builds libwebp + extras + sharpyuv with AddressSanitizer (reusing an
# existing build when present), compiles pocs/residual.c against the static
# libraries and runs it.
#
# Some environments have an unreliable ASan runtime startup (fixed-offset
# shadow mapping vs. ASLR), so the run is retried until the PoC prints its
# POC_START sentinel. After that sentinel the only code that runs is the
# library call under test, so any abort afterwards is a genuine library
# fault; the script then exits with that run's exit code.
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${POC_BUILD_DIR:-/tmp/sharpyuv_poc_build}"
BIN=/tmp/sharpyuv_poc_residual

if [ ! -f "$BUILD/libextras.a" ] || [ ! -f "$BUILD/libwebp.a" ] ||
   [ ! -f "$BUILD/libsharpyuv.a" ]; then
  cmake -S "$REPO" -B "$BUILD" -GNinja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_C_FLAGS="-fsanitize=address -O1" \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -O1" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address" \
    -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF \
    -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF \
    -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF \
    -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF \
    -DWEBP_BUILD_EXTRAS=ON || exit 1
  cmake --build "$BUILD" -j"$(nproc)" --target extras || exit 1
fi

gcc -fsanitize=address -g -O1 -I"$REPO" "$REPO/pocs/residual.c" \
  "$BUILD/libextras.a" "$BUILD/libwebp.a" "$BUILD/libsharpyuv.a" \
  -lm -lpthread -o "$BIN" || exit 1

attempt=0
while [ $attempt -lt 50 ]; do
  attempt=$((attempt + 1))
  out="$(ASAN_OPTIONS=fast_unwind_on_fatal=0 "$BIN" 2>&1)"
  ec=$?
  case "$out" in
    *POC_START*)
      printf '%s\n' "$out"
      echo "poc_exit_code=$ec"
      exit $ec
      ;;
  esac
done
echo "PoC never reached its entry point after $attempt attempts" >&2
exit 1
