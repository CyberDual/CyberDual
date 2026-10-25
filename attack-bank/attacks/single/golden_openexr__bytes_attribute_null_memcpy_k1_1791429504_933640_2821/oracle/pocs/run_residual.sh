#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${OPENEXR_ASAN_BUILD_DIR:-/tmp/openexr-asan}"

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    cmake -S "$repo_dir" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTING=OFF \
        -DOPENEXR_BUILD_TOOLS=OFF \
        -DOPENEXR_BUILD_EXAMPLES=OFF \
        -DOPENEXR_BUILD_PYTHON=OFF \
        -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer -O1 -g' \
        -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer -O1 -g' \
        -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' \
        -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address'
fi

cmake --build "$build_dir" --target OpenEXR -j 8

g++ -std=c++17 -O1 -g -fsanitize=address -fno-omit-frame-pointer \
    -I"$repo_dir/src/lib/OpenEXR" \
    -I"$build_dir/cmake" \
    -I"$repo_dir/src/lib/Iex" \
    -I"$repo_dir/src/lib/IlmThread" \
    -I"$repo_dir/src/lib/OpenEXRCore" \
    -I/usr/include/Imath \
    "$repo_dir/pocs/residual.cpp" \
    "$build_dir/src/lib/OpenEXR/libOpenEXR-4_0_d.so" \
    -Wl,-rpath,"$build_dir/src/lib/OpenEXR" \
    -o "$build_dir/residual-poc"

exec env ASAN_OPTIONS=abort_on_error=1 "$build_dir/residual-poc"
