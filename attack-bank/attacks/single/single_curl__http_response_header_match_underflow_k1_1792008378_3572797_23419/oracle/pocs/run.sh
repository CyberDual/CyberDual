#!/bin/sh
# Reproducer: builds the tree with AddressSanitizer and runs the probe.
#
# Usage: sh pocs/run.sh [build-dir]
#
# The probe hands libcurl one end of a socketpair as the connection socket
# (CURLOPT_OPENSOCKETFUNCTION + CURLOPT_SOCKOPTFUNCTION) and delivers a
# canned server response on it, then calls curl_easy_perform(). No network
# is involved.
set -e

BLD="${1:-/tmp/curl-poc-build}"

cmake -S . -B "$BLD" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DCURL_USE_SSL=OFF \
  -DCURL_USE_LIBPSL=OFF \
  -DUSE_LIBIDN2=OFF \
  -DCURL_DISABLE_LDAP=ON \
  -DCURL_DISABLE_LDAPS=ON \
  -DCURL_BROTLI=OFF \
  -DCURL_ZSTD=OFF \
  -DCURL_ZLIB=OFF \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" > /dev/null
cmake --build "$BLD" --target libcurl_static -j "$(nproc)" > /dev/null

LIB=$(find "$BLD" -name 'libcurl-d.a' -o -name 'libcurl.a' | head -n 1)
gcc -fsanitize=address -fno-omit-frame-pointer -g -O1 -Iinclude \
    pocs/newbug.c "$LIB" -o "$BLD/poc_newbug" -lpthread -ldl -lssl -lcrypto -lm

ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 "$BLD/poc_newbug"
