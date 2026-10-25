#!/usr/bin/env bash
set -euo pipefail

cd /work
if [ ! -f _build/asan_make/CMakeCache.txt ]; then
    cmake -S . -B _build/asan_make -G 'Unix Makefiles' \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
        -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
        -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' \
        -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address' \
        -DOPENEXR_FORCE_INTERNAL_IMATH=OFF \
        -DOPENEXR_FORCE_INTERNAL_DEFLATE=ON \
        -DOPENEXR_FORCE_INTERNAL_OPENJPH=ON \
        -DOPENEXR_BUILD_TOOLS=OFF \
        -DOPENEXR_BUILD_EXAMPLES=OFF \
        -DOPENEXR_BUILD_PYTHON=OFF \
        -DBUILD_TESTING=OFF
fi
cmake --build _build/asan_make -j4 --target OpenEXR
g++ -std=c++17 -fsanitize=address -fno-omit-frame-pointer -g \
    -I _build/asan_make/cmake \
    -I src/lib/OpenEXR -I src/lib/Iex -I src/lib/IlmThread \
    -I src/lib/OpenEXRCore -I /usr/include/Imath \
    pocs/residual.cpp \
    -L _build/asan_make/src/lib/OpenEXR \
    -L _build/asan_make/src/lib/OpenEXRCore \
    -L _build/asan_make/src/lib/Iex \
    -L _build/asan_make/src/lib/IlmThread \
    -Wl,-rpath,/work/_build/asan_make/src/lib/OpenEXR \
    -Wl,-rpath,/work/_build/asan_make/src/lib/OpenEXRCore \
    -Wl,-rpath,/work/_build/asan_make/src/lib/Iex \
    -Wl,-rpath,/work/_build/asan_make/src/lib/IlmThread \
    -lOpenEXR_d -lOpenEXRCore_d -lIex_d -lIlmThread_d -lImath-3_1 \
    -o pocs/residual
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 ./pocs/residual
