#!/bin/sh
set -eu
cd /work
if [ ! -f build-asan/src/lib/OpenEXRCore/libOpenEXRCore-4_0_d.so ]; then
    cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
        -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
        -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' \
        -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address' \
        -DBUILD_TESTING=OFF -DOPENEXR_BUILD_TOOLS=OFF
    cmake --build build-asan -j4 --target OpenEXR
fi
c++ -std=c++17 -fsanitize=address -fno-omit-frame-pointer -g \
    pocs/newbug.cpp -o /tmp/lj2k-newbug-probe \
    -I src/lib/OpenEXR -I src/lib/Iex -I src/lib/IlmThread \
    -I src/lib/OpenEXRCore -I build-asan/cmake -I /usr/include/Imath \
    -L build-asan/src/lib/OpenEXR -L build-asan/src/lib/OpenEXRCore \
    -L build-asan/src/lib/Iex -L build-asan/src/lib/IlmThread \
    -lOpenEXR-4_0_d -lOpenEXRCore-4_0_d -lIex-4_0_d -lIlmThread-4_0_d
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 \
LD_LIBRARY_PATH=build-asan/src/lib/OpenEXR:build-asan/src/lib/OpenEXRCore:build-asan/src/lib/Iex:build-asan/src/lib/IlmThread \
    /tmp/lj2k-newbug-probe
