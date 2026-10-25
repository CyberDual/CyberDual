#!/bin/sh
set -eu
BUILD_DIR=${PCRE2_BUILD_DIR:-/tmp/pcre2-build}
cc -fsanitize=address -fno-omit-frame-pointer \
   -I"$BUILD_DIR/interface" \
   "$(dirname "$0")/newbug.c" \
   "$BUILD_DIR/libpcre2-8.a" \
   -o /tmp/pcre2-newbug
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 /tmp/pcre2-newbug
