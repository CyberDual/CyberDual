#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
INC=""
LIB=""
if [ -n "${PCRE2_BUILD_DIR:-}" ]; then
  if [ -f "$PCRE2_BUILD_DIR/interface/pcre2.h" ]; then
    INC="$PCRE2_BUILD_DIR/interface/pcre2.h"
  elif [ -f "$PCRE2_BUILD_DIR/pcre2.h" ]; then
    INC="$PCRE2_BUILD_DIR/pcre2.h"
  fi
  if [ -f "$PCRE2_BUILD_DIR/libpcre2-8.a" ]; then
    LIB="$PCRE2_BUILD_DIR/libpcre2-8.a"
  elif [ -f "$PCRE2_BUILD_DIR/libpcre2-8.so" ]; then
    LIB="$PCRE2_BUILD_DIR/libpcre2-8.so"
  fi
fi
if [ -z "$INC" ]; then
  INC=$(find . -name pcre2.h -print -quit)
fi
if [ -z "$LIB" ]; then
  LIB=$(find . \( -name 'libpcre2-8.a' -o -name 'libpcre2-8.so' \) -print -quit)
fi
if [ -z "$INC" ] || [ -z "$LIB" ]; then
  echo "pcre2 build not found" >&2
  exit 2
fi
cc -fsanitize=address -fno-omit-frame-pointer -g -O1 -DPCRE2_CODE_UNIT_WIDTH=8 \
  -I"$(dirname "$INC")" pocs/newbug.c "$LIB" -o /tmp/poc_newbug
LD_LIBRARY_PATH="$(dirname "$LIB")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}abort_on_error=1" /tmp/poc_newbug
