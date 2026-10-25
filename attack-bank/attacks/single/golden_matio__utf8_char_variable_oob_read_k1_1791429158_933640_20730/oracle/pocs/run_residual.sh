#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

cmake -S . -B /tmp/matio-residual-asan \
    -DMATIO_MAT73=OFF -DMATIO_WITH_HDF5=OFF -DMATIO_WITH_ZLIB=OFF \
    -DMATIO_BUILD_TESTING=OFF \
    -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer -g -O1' \
    -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address' >/dev/null
cmake --build /tmp/matio-residual-asan -j4 >/dev/null
cc -fsanitize=address -fno-omit-frame-pointer -g -O1 \
    -I src -I /tmp/matio-residual-asan/src pocs/residual.c \
    -L /tmp/matio-residual-asan -Wl,-rpath,/tmp/matio-residual-asan \
    -lmatio -o /tmp/matio-residual-asan/residual
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 /tmp/matio-residual-asan/residual
