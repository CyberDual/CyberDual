#!/bin/sh
set -eu

cmake -S . -B build-asan-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_DEBUG=OFF \
  -DUSE_HTTPSRR=ON \
  -DBUILD_TESTING=ON \
  -DCURL_USE_OPENSSL=ON \
  -DCURL_USE_LIBPSL=OFF \
  -DUSE_LIBPSL=OFF \
  '-DCMAKE_C_FLAGS=-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address \
  -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address >/dev/null
cmake --build build-asan-release --target libcurl_shared -j 8 >/dev/null
cc -fsanitize=address -fno-omit-frame-pointer -g -I include \
  pocs/residual.c -L build-asan-release/lib -lcurl -lssl -lcrypto \
  -o pocs/residual
LD_LIBRARY_PATH="$PWD/build-asan-release/lib" \
  ASAN_OPTIONS=detect_leaks=0 ./pocs/residual
