#!/bin/sh
set -eu

build_dir="${TMPDIR:-/tmp}/curl-mime-asan-build"
probe="${TMPDIR:-/tmp}/curl-mime-residual"

cmake -S . -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_CURL_EXE=OFF \
  -DCURL_USE_OPENSSL=OFF \
  -DCURL_USE_LIBPSL=OFF \
  -DCURL_USE_LIBSSH2=OFF \
  -DUSE_NGHTTP2=OFF \
  -DUSE_NGHTTP3=OFF \
  -DCURL_DISABLE_LDAP=ON > /dev/null
cmake --build "$build_dir" --target libcurl_static -j 8 > /dev/null
gcc -fsanitize=address -fno-omit-frame-pointer -g -Iinclude \
  pocs/residual.c "$build_dir/lib/libcurl-d.a" \
  -lz -lbrotlidec -lbrotlicommon -lzstd -lpthread -o "$probe"
"$probe"
