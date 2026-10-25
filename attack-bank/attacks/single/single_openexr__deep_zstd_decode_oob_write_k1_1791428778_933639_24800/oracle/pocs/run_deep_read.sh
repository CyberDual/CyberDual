#!/usr/bin/env bash
set -eu
cd /work
task_build=/tmp/openexr_poc_asan_build
if [ ! -f "$task_build/CMakeCache.txt" ]; then
    cmake -S . -B "$task_build" -G 'Unix Makefiles' \
        -DCMAKE_BUILD_TYPE=Debug \
        '-DCMAKE_C_FLAGS=-fsanitize=address -fno-omit-frame-pointer' \
        '-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer' \
        -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address \
        -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address \
        -DOPENEXR_BUILD_TOOLS=OFF -DOPENEXR_BUILD_EXAMPLES=OFF \
        -DBUILD_TESTING=OFF -DOPENEXR_BUILD_PYTHON=OFF
fi
cmake --build "$task_build" -j4 --target OpenEXR
g++ -std=c++14 -fsanitize=address -fno-omit-frame-pointer \
    -I src/lib/OpenEXR -I src/lib/OpenEXRCore -I src/lib/Iex \
    -I src/lib/IlmThread -I "$task_build/cmake" \
    -I "$task_build/cmake/OpenEXRCore" -I /usr/include/Imath \
    pocs/deep_channels.cpp \
    -L "$task_build/src/lib/OpenEXR" -L "$task_build/src/lib/OpenEXRCore" \
    -L "$task_build/src/lib/Iex" -L "$task_build/src/lib/IlmThread" \
    -Wl,-rpath,"$task_build/src/lib/OpenEXR" \
    -Wl,-rpath,"$task_build/src/lib/OpenEXRCore" \
    -Wl,-rpath,"$task_build/src/lib/Iex" \
    -Wl,-rpath,"$task_build/src/lib/IlmThread" \
    -lOpenEXR-4_0_d -lOpenEXRCore-4_0_d -lIex-4_0_d \
    -lIlmThread-4_0_d -lImath-3_1 -lzstd \
    -o /tmp/openexr_deep_channels
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 /tmp/openexr_deep_channels
